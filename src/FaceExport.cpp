#include "FaceExport.h"

#include "REX/W32/D3D11.h"

#include <nlohmann/json.hpp>

// How the head is built (see the README section "FaceGen for NPC"):
//
// The Creation Kit builds a FaceGen head from each head part's own model:
// same blocks, partitions, skinning and node tree as the model, the shape
// renamed to the head part's EditorID, positions with the face morphs applied,
// texture sets baked in, and the FaceTint path in slot 6 of the face. This
// does the same in game: every effective head part's model is loaded and
// cloned, only the morphed vertex positions are taken from the player's live
// head, and the result is written with the engine's own NiStream.
//
// Nothing that belongs to a loaded model is changed for good: texture sets
// swapped for the save are put back before this returns.

namespace RMA::FaceExport
{
	namespace
	{
		namespace W = REX::W32;
		using json = nlohmann::ordered_json;
		using namespace nlohmann::literals;

		constexpr const char* kHdtKey = "HDT Skinned Mesh Physics Object";

		std::atomic_bool g_busy{ false };

		// ------------------------------------------------------------------
		// engine functions CommonLib does not wrap (SKSE 2.0.20 / 2.2.6 offsets,
		// mapped to Address Library IDs for 1.5.97 and 1.6.1170)

		void NiStreamCtor(RE::NiStream* a_this)
		{
			using func_t = RE::NiStream* (*)(RE::NiStream*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(68971, 70324) };
			func(a_this);
		}

		void NiStreamDtor(RE::NiStream* a_this)
		{
			using func_t = void (*)(RE::NiStream*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(68972, 70325) };
			func(a_this);
		}

		void NiStreamAddObject(RE::NiStream* a_this, RE::NiObject* a_object)
		{
			using func_t = void (*)(RE::NiStream*, RE::NiObject*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(68973, 70326) };
			func(a_this, a_object);
		}

		RE::BSFadeNode* CreateFadeNode()
		{
			using func_t = RE::BSFadeNode* (*)(RE::BSFadeNode*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(98844, 105481) };
			auto* memory = RE::malloc(0x158);  // sizeof(BSFadeNode) on SE and AE
			if (!memory) {
				return nullptr;
			}
			std::memset(memory, 0, 0x158);
			return func(static_cast<RE::BSFadeNode*>(memory));
		}

		bool SaveNif(RE::NiObject* a_root, const std::string& a_path)
		{
			alignas(16) std::array<std::byte, 0x700> memory{};  // NiStream is 0x620
			auto* stream = reinterpret_cast<RE::NiStream*>(memory.data());
			NiStreamCtor(stream);
			NiStreamAddObject(stream, a_root);
			const bool ok = stream->Save3(a_path.c_str());
			NiStreamDtor(stream);
			return ok;
		}

		// ------------------------------------------------------------------
		// small helpers

		std::string Lower(std::string a_text)
		{
			std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_text;
		}

		std::string Trim(std::string a_text)
		{
			const auto first = a_text.find_first_not_of(" \t\r\n\"'");
			const auto last = a_text.find_last_not_of(" \t\r\n\"'");
			return first == std::string::npos ? std::string{} : a_text.substr(first, last - first + 1);
		}

		std::optional<std::uint32_t> ParseHex(std::string a_text)
		{
			a_text = Trim(a_text);
			if (a_text.starts_with("0x") || a_text.starts_with("0X")) {
				a_text = a_text.substr(2);
			}
			if (a_text.empty() || a_text.size() > 8 || !std::ranges::all_of(a_text, [](unsigned char c) { return std::isxdigit(c) != 0; })) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(std::stoul(a_text, nullptr, 16));
		}

		std::string Name(const RE::NiObjectNET* a_object)
		{
			return a_object && a_object->name.c_str() ? std::string(a_object->name.c_str()) : std::string{};
		}

		const char* EditorID(const RE::BGSHeadPart* a_part)
		{
			return a_part && a_part->formEditorID.c_str() ? a_part->formEditorID.c_str() : "";
		}

		// the defining plugin and its local FormID as FaceGen names it
		struct FormKey
		{
			std::string   plugin;
			std::uint32_t local{ 0 };
			bool          light{ false };

			[[nodiscard]] std::string Hex() const { return std::format("{:08X}", local); }
		};

		std::optional<FormKey> KeyOf(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return std::nullopt;
			}
			const auto* file = a_form->GetFile(0);
			if (!file) {
				return std::nullopt;  // created at runtime
			}
			FormKey key;
			key.plugin = std::string(file->GetFilename());
			key.light = file->IsLight();
			key.local = key.light ? (a_form->GetFormID() & 0xFFF) : (a_form->GetFormID() & 0xFFFFFF);
			return key;
		}

		json FormJson(const RE::TESForm* a_form, const char* a_editorID = nullptr)
		{
			const auto key = KeyOf(a_form);
			if (!key) {
				return nullptr;
			}
			json j;
			j["plugin"] = key->plugin;
			j["formID"] = key->Hex();
			if (a_editorID && *a_editorID) {
				j["editorID"] = a_editorID;
			} else if (const auto* edid = a_form->GetFormEditorID(); edid && *edid) {
				j["editorID"] = edid;
			}
			return j;
		}

		json Rgb(const RE::Color& a_color)
		{
			return json::array({ a_color.red, a_color.green, a_color.blue });
		}

		// FNV-1a over a file; marks the files this tool wrote
		std::optional<std::uint64_t> HashFile(const std::filesystem::path& a_path)
		{
			std::ifstream in(a_path, std::ios::binary);
			if (!in) {
				return std::nullopt;
			}
			std::uint64_t     hash = 0xcbf29ce484222325ull;
			std::vector<char> buffer(1 << 16);
			while (in) {
				in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
				for (std::streamsize i = 0; i < in.gcount(); ++i) {
					hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
					hash *= 0x100000001b3ull;
				}
			}
			return hash;
		}

		// ------------------------------------------------------------------
		// target NPC

		RE::TESNPC* ResolveNPC(const std::string& a_query, std::string& a_error)
		{
			const auto query = Trim(a_query);
			if (query.empty()) {
				a_error = "Type an NPC EditorID or FormID";
				return nullptr;
			}

			RE::TESForm* form = nullptr;
			if (const auto sep = query.find_first_of("|:"); sep != std::string::npos) {
				auto left = Trim(query.substr(0, sep));
				auto right = Trim(query.substr(sep + 1));
				if (ParseHex(left) && !ParseHex(right)) {
					std::swap(left, right);
				}
				const auto id = ParseHex(right);
				if (!id) {
					a_error = std::format("'{}' is not a FormID", right);
					return nullptr;
				}
				auto*       handler = RE::TESDataHandler::GetSingleton();
				const auto* file = handler ? handler->LookupModByName(left) : nullptr;
				if (!file) {
					a_error = std::format("Plugin '{}' is not loaded", left);
					return nullptr;
				}
				const auto local = file->IsLight() ? (*id & 0xFFF) : (*id & 0xFFFFFF);
				form = RE::TESDataHandler::GetSingleton()->LookupForm(local, left);
			} else {
				if (const auto id = ParseHex(query); id && query.size() >= 6) {
					form = RE::TESForm::LookupByID(*id);
				}
				if (!form) {
					form = RE::TESForm::LookupByEditorID(query);
				}
				if (!form) {
					a_error = std::format("No form '{}'. NPC EditorIDs are not kept in memory by the game: use Plugin.esp|FormID", query);
					return nullptr;
				}
			}
			auto* npc = form ? form->As<RE::TESNPC>() : nullptr;
			if (!npc) {
				a_error = form ? std::format("{:08X} is not an NPC", form->GetFormID()) : "No such NPC";
				return nullptr;
			}
			if (npc->IsPlayer() || npc->GetFormID() == 0x7) {
				a_error = "That is the player record";
				return nullptr;
			}
			return npc;
		}

		// ------------------------------------------------------------------
		// effective head parts: what the engine expects as FaceGen shapes

		std::vector<RE::BGSHeadPart*> EffectiveParts(RE::TESNPC* a_npc, std::vector<std::string>& a_notes)
		{
			std::vector<RE::BGSHeadPart*> parts;
			auto                          add = [&](RE::BGSHeadPart* a_part) {
                if (a_part && std::ranges::find(parts, a_part) == parts.end()) {
                    parts.push_back(a_part);
                }
			};

			if (a_npc->HasOverlays()) {
				const auto count = a_npc->GetNumBaseOverlays();
				auto**     list = a_npc->GetBaseOverlays();
				for (std::uint32_t i = 0; list && i < count; ++i) {
					add(list[i]);
				}
			} else {
				for (std::int8_t i = 0; a_npc->headParts && i < a_npc->numHeadParts; ++i) {
					add(a_npc->headParts[i]);
				}
			}

			const auto* race = a_npc->GetRace();
			const auto  sex = a_npc->IsFemale() ? RE::SEXES::kFemale : RE::SEXES::kMale;
			const auto* face = race ? race->faceRelatedData[sex] : nullptr;
			if (race && face && face->headParts && race->data.flags.none(RE::RACE_DATA::Flag::kOverlayHeadPartList)) {
				using T = RE::BGSHeadPart::HeadPartType;
				for (const auto type : { T::kFace, T::kEyes, T::kHair, T::kFacialHair, T::kEyebrows }) {
					const bool filled = std::ranges::any_of(parts, [&](const auto* p) { return p->type == type; });
					if (filled) {
						continue;
					}
					for (auto* def : *face->headParts) {
						if (def && def->type == type) {
							add(def);
							a_notes.push_back(std::format("race default {} added", EditorID(def)));
							break;
						}
					}
				}
				for (auto* def : *face->headParts) {
					if (def && def->type == T::kMisc && std::ranges::find(parts, def) == parts.end()) {
						add(def);
						a_notes.push_back(std::format("race default {} (Misc) added", EditorID(def)));
					}
				}
			}

			// extra parts, recursively
			for (std::size_t i = 0; i < parts.size(); ++i) {
				for (auto* extra : parts[i]->extraParts) {
					add(extra);
				}
			}

			// a part without a model never becomes a shape
			std::erase_if(parts, [](RE::BGSHeadPart* a_part) {
				const char* model = a_part->GetModel();
				return !model || !*model;
			});
			return parts;
		}

		// ------------------------------------------------------------------
		// building the head

		struct Built
		{
			RE::NiPointer<RE::BSFadeNode>          root;
			RE::NiPointer<RE::NiNode>              skinned;
			std::unordered_map<std::string, RE::NiNode*> bones;  // lower-case name
			std::vector<RE::NiPointer<RE::NiNode>> models;       // clones, kept alive until saved
			std::vector<std::function<void()>>     undo;
			std::vector<std::string>               shapes;
			std::vector<std::string>               log;
			std::unordered_map<RE::BSGeometry*, RE::BSGeometry*> sourceOf;  // clone -> loaded model
		};

		void ForEachGeometry(RE::NiAVObject* a_object, const std::function<void(RE::BSGeometry*)>& a_fn)
		{
			if (!a_object) {
				return;
			}
			if (auto* geometry = a_object->AsGeometry()) {
				a_fn(geometry);
				return;
			}
			if (auto* node = a_object->AsNode()) {
				for (auto& child : node->GetChildren()) {
					ForEachGeometry(child.get(), a_fn);
				}
			}
		}

		// mirrors the model's node tree under the head's root, one node per name
		void MergeBones(Built& a_out, RE::NiNode* a_source, RE::NiNode* a_target)
		{
			for (auto& child : a_source->GetChildren()) {
				auto* node = child ? child->AsNode() : nullptr;
				if (!node || child->AsGeometry()) {
					continue;
				}
				const auto key = Lower(Name(node));
				RE::NiNode* out = nullptr;
				if (const auto it = a_out.bones.find(key); it != a_out.bones.end()) {
					out = it->second;
				} else {
					out = RE::NiNode::Create(0);
					out->name = node->name;
					out->local = node->local;
					out->GetFlags() = node->GetFlags();
					a_target->AttachChild(out, true);
					a_out.bones.emplace(key, out);
				}
				MergeBones(a_out, node, out);
			}
		}

		RE::NiNode* BoneFor(Built& a_out, const RE::NiAVObject* a_bone)
		{
			const auto name = Name(a_bone);
			const auto key = Lower(name);
			if (const auto it = a_out.bones.find(key); it != a_out.bones.end()) {
				return it->second;
			}
			auto* out = RE::NiNode::Create(0);
			out->name = a_bone->name;
			out->local = a_bone->local;
			a_out.root->AttachChild(out, true);
			a_out.bones.emplace(key, out);
			a_out.log.push_back(std::format("bone '{}' is not in the model's node tree; added under the root", name));
			return out;
		}

		RE::BSLightingShaderMaterialBase* MaterialOf(RE::BSGeometry* a_geometry)
		{
			auto* property = a_geometry ? a_geometry->GetGeometryRuntimeData().shaderProperty.get() : nullptr;
			auto* lighting = property ? netimmerse_cast<RE::BSLightingShaderProperty*>(property) : nullptr;
			return lighting ? static_cast<RE::BSLightingShaderMaterialBase*>(lighting->material) : nullptr;
		}

		// gives the shape its own texture set for the save, then puts the old one back
		void SetTextures(Built& a_out, RE::BSGeometry* a_geometry, const RE::BGSTextureSet* a_txst, const std::string& a_tintPath)
		{
			auto* material = MaterialOf(a_geometry);
			if (!material || (!a_txst && a_tintPath.empty())) {
				return;
			}
			auto* fresh = RE::BSShaderTextureSet::Create();
			if (!fresh) {
				return;
			}
			using Slot = RE::BSTextureSet::Texture;
			const auto old = material->textureSet;
			for (std::uint32_t i = 0; i < Slot::kTotal; ++i) {
				const char* path = old ? old->GetTexturePath(static_cast<Slot>(i)) : nullptr;
				fresh->SetTexturePath(static_cast<Slot>(i), path ? path : "");
			}
			if (a_txst) {
				for (std::uint32_t i = 0; i < Slot::kTotal; ++i) {
					const char* path = const_cast<RE::BGSTextureSet*>(a_txst)->GetTexturePath(static_cast<Slot>(i));
					if (path && *path) {
						// TXST paths are relative to textures\, NIF paths include it
						std::string full = path;
						if (!Lower(full).starts_with("textures\\")) {
							full = "textures\\" + full;
						}
						fresh->SetTexturePath(static_cast<Slot>(i), full.c_str());
					}
				}
			}
			if (!a_tintPath.empty()) {
				fresh->SetTexturePath(static_cast<Slot>(6), a_tintPath.c_str());
			}
			material->textureSet = RE::NiPointer<RE::BSTextureSet>(fresh);
			a_out.undo.emplace_back([material, old] { material->textureSet = old; });
		}

		// A FaceGen head carries its hair colour in each HairTint shader (the
		// engine does not recolour a loaded FaceGen head). The player's live
		// shape already holds the colour the engine applied (HCLF / 128), so the
		// clone takes that value for the save; the shared material gets its own
		// value back afterwards.
		void CopyHairTint(Built& a_out, RE::BSGeometry* a_clone, RE::NiAVObject* a_live)
		{
			auto* mine = MaterialOf(a_clone);
			auto* theirs = MaterialOf(a_live ? a_live->AsGeometry() : nullptr);
			if (!mine || !theirs || mine->GetFeature() != RE::BSShaderMaterial::Feature::kHairTint ||
				theirs->GetFeature() != RE::BSShaderMaterial::Feature::kHairTint) {
				return;
			}
			auto*      dst = static_cast<RE::BSLightingShaderMaterialHairTint*>(mine);
			const auto src = static_cast<RE::BSLightingShaderMaterialHairTint*>(theirs)->tintColor;
			const auto old = dst->tintColor;
			dst->tintColor = src;
			a_out.undo.emplace_back([dst, old] { dst->tintColor = old; });
			a_out.log.push_back(std::format("'{}': hair tint {:.3f} {:.3f} {:.3f}", Name(a_clone), src.red, src.green, src.blue));
		}

		// the player's live, morphed positions into the clone
		bool CopyMorphedPositions(Built& a_out, RE::BSGeometry* a_clone, RE::NiAVObject* a_live, std::string& a_note)
		{
			auto* dst = a_clone->AsDynamicTriShape();
			auto* src = a_live ? a_live->AsDynamicTriShape() : nullptr;
			if (!dst) {
				a_note = "static shape, model positions";
				return true;
			}
			if (!src) {
				a_note = "not on the player, model positions";
				return false;
			}
			const auto dstCount = a_clone->AsTriShape()->GetTrishapeRuntimeData().vertexCount;
			const auto srcCount = a_live->AsTriShape()->GetTrishapeRuntimeData().vertexCount;
			auto&      d = dst->GetDynamicTrishapeRuntimeData();
			auto&      s = src->GetDynamicTrishapeRuntimeData();
			if (dstCount != srcCount || !d.dynamicData || !s.dynamicData || d.dynamicData == s.dynamicData) {
				a_note = std::format("vertex mismatch (model {}, player {}), model positions", dstCount, srcCount);
				return false;
			}
			// the copy must not write into the loaded model's own buffer
			const auto it = a_out.sourceOf.find(a_clone);
			auto*      original = it != a_out.sourceOf.end() && it->second ? it->second->AsDynamicTriShape() : nullptr;
			if (original && original->GetDynamicTrishapeRuntimeData().dynamicData == d.dynamicData) {
				auto* own = RE::aligned_alloc(16, static_cast<std::size_t>(dstCount) * 16);
				if (!own) {
					a_note = "out of memory, model positions";
					return false;
				}
				void* shared = d.dynamicData;
				d.dynamicData = own;
				a_out.undo.emplace_back([&d, shared, own] {
					d.dynamicData = shared;
					RE::aligned_free(own);
				});
			}
			{
				RE::BSSpinLockGuard guard(s.lock);
				std::memcpy(d.dynamicData, s.dynamicData, static_cast<std::size_t>(dstCount) * 16);
			}
			a_note = std::format("{} morphed vertices", dstCount);
			return true;
		}

		RE::BGSTextureSet* TextureSetFor(RE::TESNPC* a_npc, RE::BGSHeadPart* a_part)
		{
			if (a_part->type == RE::BGSHeadPart::HeadPartType::kFace && a_npc->headRelatedData && a_npc->headRelatedData->faceDetails) {
				return a_npc->headRelatedData->faceDetails;
			}
			return a_part->textureSet;
		}

		bool BuildHead(Built& a_out, RE::Actor* a_player, RE::TESNPC* a_base, const std::vector<RE::BGSHeadPart*>& a_parts,
			const FormKey& a_target, std::string& a_error)
		{
			auto* faceNode = a_player->GetFaceNodeSkinned();
			if (!faceNode) {
				a_error = "The player has no face node";
				return false;
			}

			a_out.root = RE::NiPointer<RE::BSFadeNode>(CreateFadeNode());
			if (!a_out.root) {
				a_error = "Could not create the head's root node";
				return false;
			}
			a_out.root->name = std::format("{}.NIF", a_target.Hex());
			a_out.skinned = RE::NiPointer<RE::NiNode>(RE::NiNode::Create(0));
			a_out.skinned->name = "BSFaceGenNiNodeSkinned";

			const auto tintPath = std::format("textures\\actors\\character\\facegendata\\facetint\\{}\\{}.dds", Lower(a_target.plugin), a_target.Hex());

			std::vector<std::string> hdtSeen;
			std::vector<std::pair<RE::NiPointer<RE::BSGeometry>, RE::BGSHeadPart*>> placed;

			for (auto* part : a_parts) {
				const std::string edid = EditorID(part);
				const char*       modelPath = part->GetModel();

				RE::NiPointer<RE::NiNode>              source;
				RE::BSModelDB::DBTraits::ArgsType      args{};
				args.postProcess = false;
				if (RE::BSModelDB::Demand(modelPath, source, args) != RE::BSResource::ErrorCode::kNone || !source) {
					a_error = std::format("Could not load the model of {} ({})", edid, modelPath);
					return false;
				}
				RE::NiPointer<RE::NiObject> cloneObject(source->Clone());
				RE::NiPointer<RE::NiNode>   clone(cloneObject ? cloneObject->AsNode() : nullptr);
				if (!clone) {
					a_error = std::format("Could not copy the model of {} ({})", edid, modelPath);
					return false;
				}
				a_out.models.push_back(clone);

				// physics XML and other string data the model's root carries
				for (std::uint16_t i = 0; i < clone->GetExtraDataSize(); ++i) {
					auto* extra = clone->GetExtraDataAt(i);
					auto* text = extra ? netimmerse_cast<RE::NiStringExtraData*>(extra) : nullptr;
					if (!text || !text->GetName().c_str() || std::string_view(text->GetName().c_str()) != kHdtKey || !text->value) {
						continue;
					}
					const auto value = Lower(text->value);
					if (std::ranges::find(hdtSeen, value) == hdtSeen.end()) {
						hdtSeen.push_back(value);
						a_out.root->AddExtraData(text);
						a_out.log.push_back(std::format("{}: physics XML {}", edid, text->value));
					}
				}

				MergeBones(a_out, clone.get(), a_out.root.get());

				std::vector<RE::BSGeometry*> geometries;
				std::vector<RE::BSGeometry*> originals;
				ForEachGeometry(clone.get(), [&](RE::BSGeometry* g) { geometries.push_back(g); });
				ForEachGeometry(source.get(), [&](RE::BSGeometry* g) { originals.push_back(g); });
				if (originals.size() != geometries.size()) {
					a_error = std::format("The copy of {} does not match its model", modelPath);
					return false;
				}
				for (std::size_t i = 0; i < geometries.size(); ++i) {
					a_out.sourceOf[geometries[i]] = originals[i];
				}
				if (geometries.empty()) {
					a_out.log.push_back(std::format("{}: model {} has no shapes", edid, modelPath));
					continue;
				}

				// one shape: it is the part. Several: the engine keeps their own
				// names and renames the one it uses as the part; the player's
				// face node shows which ones it attached.
				RE::BSGeometry* primary = nullptr;
				if (geometries.size() == 1) {
					primary = geometries.front();
				} else {
					auto* live = faceNode->GetObjectByName(RE::BSFixedString(edid.c_str()));
					const auto liveCount = live && live->AsTriShape() ? live->AsTriShape()->GetTrishapeRuntimeData().vertexCount : 0;
					for (auto* g : geometries) {
						const bool attachedByName = faceNode->GetObjectByName(g->name) != nullptr;
						if (!attachedByName && g->AsTriShape() && g->AsTriShape()->GetTrishapeRuntimeData().vertexCount == liveCount) {
							primary = g;
							break;
						}
					}
				}

				for (auto* g : geometries) {
					const bool isPrimary = g == primary;
					if (!isPrimary && !faceNode->GetObjectByName(g->name)) {
						a_out.log.push_back(std::format("{}: model shape '{}' is not on the player, left out", edid, Name(g)));
						continue;
					}
					if (isPrimary) {
						g->name = RE::BSFixedString(edid.c_str());
					}
					placed.emplace_back(RE::NiPointer<RE::BSGeometry>(g), isPrimary ? part : nullptr);
					CopyHairTint(a_out, g, faceNode->GetObjectByName(g->name));
					if (isPrimary) {
						SetTextures(a_out, g, TextureSetFor(a_base, part),
							part->type == RE::BGSHeadPart::HeadPartType::kFace ? tintPath : std::string{});
					}
				}
				if (!primary) {
					a_out.log.push_back(std::format("{}: no model shape matches the player's '{}', the part has no shape of its own name", edid, edid));
				}
			}

			// move the shapes under BSFaceGenNiNodeSkinned and bind them to the new bones
			for (auto& [g, part] : placed) {
				if (auto* parent = g->parent) {
					parent->DetachChild(g.get());
				}
				auto* live = faceNode->GetObjectByName(g->name);
				std::string note;
				const bool  morphed = CopyMorphedPositions(a_out, g.get(), live, note);

				if (auto* skin = g->GetGeometryRuntimeData().skinInstance.get()) {
					const auto  it = a_out.sourceOf.find(g.get());
					const auto* original = it != a_out.sourceOf.end() ? it->second : nullptr;
					if (original && original->GetGeometryRuntimeData().skinInstance.get() == skin) {
						a_error = std::format("The copy of '{}' shares its skin with the loaded model", Name(g.get()));
						return false;
					}
					const auto count = skin->skinData ? skin->skinData->bones : 0;
					for (std::uint32_t i = 0; skin->bones && i < count; ++i) {
						if (skin->bones[i]) {
							skin->bones[i] = BoneFor(a_out, skin->bones[i]);
						}
					}
					skin->rootParent = a_out.skinned.get();
				}

				a_out.skinned->AttachChild(g.get(), true);
				const auto vertices = g->AsTriShape() ? g->AsTriShape()->GetTrishapeRuntimeData().vertexCount : 0;
				a_out.shapes.push_back(Name(g.get()));
				a_out.log.push_back(std::format("shape '{}' ({}, {} vertices): {}{}", Name(g.get()), g->GetRTTI() ? g->GetRTTI()->GetName() : "?",
					vertices, note, morphed ? "" : "  <- check"));
			}
			a_out.root->AttachChild(a_out.skinned.get(), true);

			// shapes the player shows that no effective part explains (overlays, physics helpers)
			for (auto& child : faceNode->GetChildren()) {
				if (child && child->AsGeometry() && std::ranges::find(a_out.shapes, Name(child.get())) == a_out.shapes.end()) {
					a_out.log.push_back(std::format("player shape '{}' left out (not a head part model shape)", Name(child.get())));
				}
			}
			return !placed.empty() || (a_error = "No shapes to write", false);
		}

		// ------------------------------------------------------------------
		// face tint

		bool WriteDDS(const std::filesystem::path& a_path, W::ID3D11Texture2D* a_texture, std::string& a_info)
		{
			auto* device = RE::BSGraphics::Renderer::GetDevice();
			auto* data = RE::BSGraphics::Renderer::GetRendererDataSingleton();
			auto* context = data ? data->context : nullptr;
			if (!device || !context || !a_texture) {
				a_info = "no renderer";
				return false;
			}
			W::D3D11_TEXTURE2D_DESC desc{};
			a_texture->GetDesc(&desc);

			std::uint32_t r = 0, g = 0, b = 0, a = 0;
			switch (desc.format) {
			case W::DXGI_FORMAT_R8G8B8A8_UNORM:
			case W::DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				r = 0x000000FF, g = 0x0000FF00, b = 0x00FF0000, a = 0xFF000000;
				break;
			case W::DXGI_FORMAT_B8G8R8A8_UNORM:
			case W::DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
				r = 0x00FF0000, g = 0x0000FF00, b = 0x000000FF, a = 0xFF000000;
				break;
			default:
				a_info = std::format("tint format {} is not handled", static_cast<int>(desc.format));
				return false;
			}

			W::D3D11_TEXTURE2D_DESC staging = desc;
			staging.mipLevels = desc.mipLevels;
			staging.arraySize = 1;
			staging.sampleDesc.count = 1;
			staging.sampleDesc.quality = 0;
			staging.usage = W::D3D11_USAGE_STAGING;
			staging.bindFlags = 0;
			staging.cpuAccessFlags = W::D3D11_CPU_ACCESS_READ;
			staging.miscFlags = 0;
			W::ID3D11Texture2D* copy = nullptr;
			if (device->CreateTexture2D(&staging, nullptr, &copy) < 0 || !copy) {
				a_info = "CreateTexture2D failed";
				return false;
			}
			context->CopyResource(copy, a_texture);
			W::D3D11_MAPPED_SUBRESOURCE mapped{};
			if (context->Map(copy, 0, W::D3D11_MAP_READ, 0, &mapped) < 0 || !mapped.data) {
				copy->Release();
				a_info = "Map failed";
				return false;
			}

			std::vector<std::uint32_t> header(32, 0);
			header[0] = 0x20534444;  // "DDS "
			header[1] = 124;
			header[2] = 0x1 | 0x2 | 0x4 | 0x8 | 0x1000;  // caps, height, width, pitch, pixel format
			header[3] = desc.height;
			header[4] = desc.width;
			header[5] = desc.width * 4;
			header[7] = 1;
			header[19] = 32;           // pixel format size
			header[20] = 0x40 | 0x1;   // RGB | alpha pixels
			header[22] = 32;
			header[23] = r;
			header[24] = g;
			header[25] = b;
			header[26] = a;
			header[27] = 0x1000;  // texture

			bool ok = false;
			{
				std::ofstream out(a_path, std::ios::binary | std::ios::trunc);
				if (out) {
					out.write(reinterpret_cast<const char*>(header.data()), 128);
					const auto* bytes = static_cast<const char*>(mapped.data);
					for (std::uint32_t row = 0; row < desc.height; ++row) {
						out.write(bytes + static_cast<std::size_t>(row) * mapped.rowPitch, static_cast<std::streamsize>(desc.width) * 4);
					}
					ok = static_cast<bool>(out);
				}
			}
			context->Unmap(copy, 0);
			copy->Release();
			a_info = std::format("{}x{}", desc.width, desc.height);
			return ok;
		}

		bool ExportTint(RE::Actor* a_player, RE::TESNPC* a_base, const std::filesystem::path& a_path, std::string& a_info)
		{
			auto* faceNode = a_player->GetFaceNodeSkinned();
			auto* facePart = a_base->GetCurrentHeadPartByType(RE::BGSHeadPart::HeadPartType::kFace);
			auto* object = faceNode && facePart ? faceNode->GetObjectByName(facePart->formEditorID) : nullptr;
			auto* material = MaterialOf(object ? object->AsGeometry() : nullptr);
			if (!material || material->GetFeature() != RE::BSShaderMaterial::Feature::kFaceGen) {
				a_info = "the player's face has no FaceGen material";
				return false;
			}
			auto* facegen = static_cast<RE::BSLightingShaderMaterialFacegen*>(material);
			auto* tint = facegen->tintTexture.get();
			auto* renderer = tint ? reinterpret_cast<RE::NiTexture::RendererData*>(tint->rendererTexture) : nullptr;
			if (!renderer) {
				a_info = "the player's face tint is not rendered";
				return false;
			}
			auto* texture = reinterpret_cast<W::ID3D11Texture2D*>(renderer->texture);
			if (!texture && renderer->resourceView) {
				W::ID3D11Resource* resource = nullptr;
				reinterpret_cast<W::ID3D11ShaderResourceView*>(renderer->resourceView)->GetResource(&resource);
				texture = reinterpret_cast<W::ID3D11Texture2D*>(resource);  // view of a 2D texture
				const bool ok = WriteDDS(a_path, texture, a_info);
				if (resource) {
					resource->Release();
				}
				return ok;
			}
			return WriteDDS(a_path, texture, a_info);
		}

		// ------------------------------------------------------------------
		// record data for the xEdit script

		json RecordJson(RE::PlayerCharacter* a_player, RE::TESNPC* a_base, const std::vector<RE::BGSHeadPart*>& a_effective)
		{
			json j;
			auto* race = a_base->GetRace();
			j["race"] = FormJson(race);
			j["female"] = a_base->IsFemale();

			// PNAM: the player's own list; race defaults stay implicit, as in the CK
			json parts = json::array();
			for (std::int8_t i = 0; a_base->headParts && i < a_base->numHeadParts; ++i) {
				auto* part = a_base->headParts[i];
				if (!part) {
					continue;
				}
				auto p = FormJson(part, EditorID(part));
				if (p.is_null()) {
					p = json{ { "editorID", EditorID(part) }, { "runtime", true } };
				}
				p["type"] = static_cast<int>(part->type.underlying());
				parts.push_back(std::move(p));
			}
			j["headParts"] = std::move(parts);

			json effective = json::array();
			for (auto* part : a_effective) {
				effective.push_back(EditorID(part));
			}
			j["effectiveHeadParts"] = std::move(effective);

			// HCLF
			if (auto* hair = a_base->headRelatedData ? a_base->headRelatedData->hairColor : nullptr) {
				j["hairColor"] = { { "form", FormJson(hair) }, { "rgb", Rgb(hair->color) } };
			} else {
				j["hairColor"] = nullptr;
			}
			// FTST
			j["faceTextureSet"] = a_base->headRelatedData && a_base->headRelatedData->faceDetails ? FormJson(a_base->headRelatedData->faceDetails) : json(nullptr);

			// NAM9 / NAMA
			if (a_base->faceData) {
				json morphs = json::array();
				for (const float m : a_base->faceData->morphs) {
					morphs.push_back(std::isfinite(m) ? m : 3.4028235e38f);
				}
				j["morphs"] = std::move(morphs);
				j["faceParts"] = json::array({ a_base->faceData->parts[0], a_base->faceData->parts[1], a_base->faceData->parts[2], a_base->faceData->parts[3] });
			} else {
				j["morphs"] = nullptr;
				j["faceParts"] = nullptr;
			}

			// tint layers: the player's live masks, mapped back to the race's tint assets
			json         layers = json::array();
			RE::Color    skinTone = a_base->bodyTintColor;
			bool         haveSkinTone = false;
			const auto*  faceData = race ? race->faceRelatedData[a_base->IsFemale() ? RE::SEXES::kFemale : RE::SEXES::kMale] : nullptr;
			auto&        masks = REL::RelocateMemberIfNewer<RE::BSTArray<RE::TintMask*>>(SKSE::RUNTIME_SSE_1_6_629, a_player, 0xB10, 0xB18);
			for (auto* mask : masks) {
				if (!mask || !mask->texture) {
					continue;
				}
				const auto path = Lower(mask->texture->textureName.c_str() ? mask->texture->textureName.c_str() : "");
				const RE::TESRace::FaceRelatedData::TintAsset* asset = nullptr;
				if (faceData && faceData->tintMasks) {
					for (auto* candidate : *faceData->tintMasks) {
						const char* name = candidate ? candidate->texture.file.textureName.c_str() : nullptr;
						if (name && Lower(name) == path) {
							asset = candidate;
							break;
						}
					}
				}
				if (!asset) {
					continue;  // a mask the race does not list cannot be a record layer
				}
				std::int32_t preset = -1;
				const auto&  colors = asset->presets.colors;
				for (std::uint32_t k = 0; k < colors.size(); ++k) {
					const auto* form = colors[k];
					if (form && form->color.red == mask->color.red && form->color.green == mask->color.green && form->color.blue == mask->color.blue) {
						preset = k < asset->presets.indices.size() ? asset->presets.indices[k] : -1;
						break;
					}
				}
				const auto alpha = std::clamp(mask->alpha, 0.0f, 1.0f);
				layers.push_back({ { "index", asset->texture.index },
					{ "type", static_cast<int>(mask->type.underlying()) },
					{ "color", Rgb(mask->color) },
					{ "value", std::round(alpha * 100.0f) / 100.0f },
					{ "preset", preset },
					{ "texture", path } });
				if (mask->type.all(RE::TintMask::Type::kSkinTone) && !haveSkinTone) {
					skinTone = mask->color;
					haveSkinTone = true;
				}
			}
			j["tintLayers"] = std::move(layers);
			j["skinTone"] = Rgb(skinTone);

			j["height"] = a_base->height;
			j["weight"] = a_base->weight;

			// scale the record cannot carry: RaceMenu/NiOverride node scales
			json scale;
			scale["reference"] = a_player->GetScale();
			if (auto* root = a_player->Get3D(false)) {
				for (const char* nodeName : { "NPC", "NPC Root [Root]", "NPC Head [Head]" }) {
					if (auto* node = root->GetObjectByName(nodeName)) {
						scale[nodeName] = node->local.scale;
					}
				}
			}
			j["playerScale"] = std::move(scale);
			return j;
		}

		// ------------------------------------------------------------------
		// the export

		struct Job
		{
			std::string query;
			StatusFn    status;
		};

		void Finish(const std::shared_ptr<Job>& a_job, std::string a_text)
		{
			logger::info("FaceGen for NPC: {}", a_text);
			if (a_job->status) {
				a_job->status(std::move(a_text));
			}
			g_busy.store(false);
		}

		// the engine's face morph update (SKSE's UpdateModelFace)
		void UpdateModelFace(RE::NiAVObject* a_faceNode)
		{
			using func_t = std::uint32_t (*)(RE::NiAVObject*);
			static REL::Relocation<func_t> func{ RELOCATION_ID(26458, 27044) };
			func(a_faceNode);
		}

		// RaceMenu's own head export does the same: every expression, modifier,
		// phoneme and custom value to zero, then the morphs are recomputed at
		// once, so the positions read right after carry no expression. Waiting
		// for a neutral face instead never ends while blink and idle face
		// animation mods keep moving it.
		void NeutralizeFace(RE::Actor* a_actor)
		{
			auto* data = a_actor->GetFaceGenAnimationData();
			auto* faceNode = a_actor->GetFaceNodeSkinned();
			if (!data || !faceNode) {
				return;
			}
			for (auto* frame : { &data->expressionKeyFrame, &data->expressionKeyFrame2, &data->modifierKeyFrame, &data->phenomeKeyFrame,
					 &data->customKeyFrame, &data->expression3, &data->modifier1, &data->modifier3, &data->phoneme1, &data->phoneme3,
					 &data->custom1, &data->custom3 }) {
				for (std::uint32_t i = 0; frame->values && i < frame->count; ++i) {
					frame->values[i] = 0.0f;
				}
				frame->isUpdated = false;
			}
			UpdateModelFace(faceNode);
		}

		void Export(const std::shared_ptr<Job>& a_job)
		{
			std::string error;
			auto*       npc = ResolveNPC(a_job->query, error);
			if (!npc) {
				Finish(a_job, error);
				return;
			}
			const auto target = KeyOf(npc);
			if (!target) {
				Finish(a_job, "That NPC was created at runtime and has no plugin");
				return;
			}
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* base = player ? player->GetActorBase() : nullptr;
			if (!base || !player->Get3D(false)) {
				Finish(a_job, "The player has no 3D");
				return;
			}

			std::vector<std::string> warnings;
			if (npc->GetRace() != base->GetRace()) {
				warnings.push_back(std::format("race differs: player {}, NPC {}", base->GetRace() ? base->GetRace()->GetFormEditorID() : "?", npc->GetRace() ? npc->GetRace()->GetFormEditorID() : "?"));
			}
			if (npc->IsFemale() != base->IsFemale()) {
				warnings.push_back("sex differs from the player");
			}
			if (npc->UsesTemplate() && npc->actorData.templateUseFlags.any(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kTraits)) {
				warnings.push_back("the NPC uses a template's traits; the game will show the template's FaceGen");
			}

			const auto meshDir = std::filesystem::path("Data\\meshes\\actors\\character\\FaceGenData\\FaceGeom") / target->plugin;
			const auto tintDir = std::filesystem::path("Data\\textures\\actors\\character\\FaceGenData\\FaceTint") / target->plugin;
			const auto jsonDir = std::filesystem::path("Data\\SKSE\\Plugins\\RaceMenuAtelier\\NPCFaces") / target->plugin;
			const auto nifPath = meshDir / (target->Hex() + ".nif");
			const auto ddsPath = tintDir / (target->Hex() + ".dds");
			const auto jsonPath = jsonDir / (target->Hex() + ".json");

			// never overwrite a FaceGen file some mod ships: under MO2 that would
			// edit the mod's own file in place. Our own earlier export is fine.
			std::uint64_t ownNif = 0, ownDds = 0;
			{
				std::ifstream previous(jsonPath);
				if (previous) {
					try {
						const auto old = json::parse(previous);
						ownNif = std::stoull(old.value("/written/nifHash"_json_pointer, std::string("0")), nullptr, 16);
						ownDds = std::stoull(old.value("/written/ddsHash"_json_pointer, std::string("0")), nullptr, 16);
					} catch (...) {
					}
				}
			}
			for (const auto& [path, own] : { std::pair{ nifPath, ownNif }, std::pair{ ddsPath, ownDds } }) {
				std::error_code ec;
				if (std::filesystem::exists(path, ec)) {
					const auto hash = HashFile(path);
					if (!hash || *hash != own) {
						Finish(a_job, std::format("{} already exists and was not written by this tool - move it out of its mod first", path.string()));
						return;
					}
				}
			}

			std::vector<std::string> notes;
			const auto               parts = EffectiveParts(base, notes);
			if (parts.empty()) {
				Finish(a_job, "The player has no head parts with models");
				return;
			}

			NeutralizeFace(player);

			Built built;
			if (!BuildHead(built, player, base, parts, *target, error)) {
				for (auto& undo : built.undo) {
					undo();
				}
				Finish(a_job, error);
				return;
			}

			std::error_code ec;
			std::filesystem::create_directories(meshDir, ec);
			std::filesystem::create_directories(tintDir, ec);
			std::filesystem::create_directories(jsonDir, ec);

			const bool nifOk = SaveNif(built.root.get(), nifPath.string());
			for (auto it = built.undo.rbegin(); it != built.undo.rend(); ++it) {
				(*it)();
			}
			built.undo.clear();
			if (!nifOk || !std::filesystem::exists(nifPath, ec)) {
				Finish(a_job, std::format("Saving {} failed", nifPath.string()));
				return;
			}

			std::string tintInfo;
			const bool  ddsOk = ExportTint(player, base, ddsPath, tintInfo);

			json record = RecordJson(player, base, parts);
			json doc;
			doc["format"] = "RaceMenuAtelier.NPCFace";
			doc["version"] = 1;
			doc["npc"] = FormJson(npc);
			doc["record"] = std::move(record);
			doc["faceGenShapes"] = built.shapes;
			doc["warnings"] = warnings;
			json written;
			written["nif"] = nifPath.string().substr(5);  // without "Data\"
			written["nifHash"] = std::format("{:016X}", HashFile(nifPath).value_or(0));
			written["dds"] = ddsOk ? json(ddsPath.string().substr(5)) : json(nullptr);
			written["ddsHash"] = ddsOk ? json(std::format("{:016X}", HashFile(ddsPath).value_or(0))) : json(nullptr);
			written["tint"] = tintInfo;
			doc["written"] = std::move(written);
			{
				std::ofstream out(jsonPath, std::ios::binary | std::ios::trunc);
				out << doc.dump(2) << '\n';
			}

			logger::info("FaceGen for NPC {}|{}: {} shapes", target->plugin, target->Hex(), built.shapes.size());
			for (const auto& line : notes) {
				logger::info("  {}", line);
			}
			for (const auto& line : built.log) {
				logger::info("  {}", line);
			}
			for (const auto& line : warnings) {
				logger::warn("  {}", line);
			}
			Finish(a_job, std::format("FaceGen for {}|{} written: {} shapes, tint {}{}", target->plugin, target->Hex(), built.shapes.size(),
				ddsOk ? tintInfo : "FAILED (" + tintInfo + ")", warnings.empty() ? "" : " - see the log, " + warnings.front()));
		}
	}

	bool IsBusy()
	{
		return g_busy.load();
	}

	namespace
	{
		std::mutex                                     g_npcLock;
		std::shared_ptr<const std::vector<NPCEntry>> g_npcs;
		std::atomic_bool                               g_npcsPending{ false };

		void BuildNPCList()
		{
			auto list = std::make_shared<std::vector<NPCEntry>>();
			if (auto* handler = RE::TESDataHandler::GetSingleton()) {
				for (auto* npc : handler->GetFormArray<RE::TESNPC>()) {
					if (!npc || npc->IsPlayer() || npc->GetFormID() == 0x7) {
						continue;
					}
					const auto key = KeyOf(npc);
					if (!key) {
						continue;
					}
					NPCEntry e;
					e.plugin = key->plugin;
					e.formID = key->Hex();
					e.name = npc->GetName() ? npc->GetName() : "";
					e.editorID = npc->GetFormEditorID() ? npc->GetFormEditorID() : "";
					e.female = npc->IsFemale();
					e.templated = npc->UsesTemplate() && npc->actorData.templateUseFlags.any(RE::ACTOR_BASE_DATA::TEMPLATE_USE_FLAG::kTraits);
					e.search = Lower(std::format("{} {} {} {} {:08X}", e.name, e.editorID, e.formID, e.plugin, npc->GetFormID()));
					list->push_back(std::move(e));
				}
			}
			std::ranges::sort(*list, [](const NPCEntry& a, const NPCEntry& b) {
				const bool an = a.name.empty(), bn = b.name.empty();
				if (an != bn) {
					return bn;  // named NPCs first
				}
				return a.search < b.search;  // starts with the lower-case name
			});
			logger::info("FaceGen for NPC: {} NPC records listed", list->size());
			{
				std::scoped_lock lock(g_npcLock);
				g_npcs = std::move(list);
			}
			g_npcsPending = false;
		}
	}

	void RequestNPCList()
	{
		if (g_npcsPending.exchange(true)) {
			return;
		}
		SKSE::GetTaskInterface()->AddTask([] { BuildNPCList(); });
	}

	std::shared_ptr<const std::vector<NPCEntry>> NPCList()
	{
		std::scoped_lock lock(g_npcLock);
		return g_npcs;
	}

	bool NPCListPending()
	{
		return g_npcsPending.load();
	}

	void Start(std::string a_query, StatusFn a_status)
	{
		if (g_busy.exchange(true)) {
			if (a_status) {
				a_status("A FaceGen export is already running");
			}
			return;
		}
		auto job = std::make_shared<Job>();
		job->query = std::move(a_query);
		job->status = std::move(a_status);
		SKSE::GetTaskInterface()->AddTask([job] { Export(job); });
	}
}
