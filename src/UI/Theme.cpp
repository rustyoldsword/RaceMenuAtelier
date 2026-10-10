#include "UI/Theme.h"

#include "Settings.h"

namespace RMA::UI
{
	namespace
	{
		constexpr int kColors = 30;
		constexpr int kVars = 17;
	}

	namespace
	{
		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) {
				a_text.remove_prefix(1);
			}
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) {
				a_text.remove_suffix(1);
			}
			return a_text;
		}

		// #RRGGBB or #RRGGBBAA
		std::optional<ImU32> ParseColor(std::string_view a_text)
		{
			a_text = Trim(a_text);
			if (!a_text.empty() && a_text.front() == '#') {
				a_text.remove_prefix(1);
			}
			if (a_text.size() != 6 && a_text.size() != 8) {
				return std::nullopt;
			}
			std::uint32_t v = 0;
			const auto [end, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), v, 16);
			if (ec != std::errc{} || end != a_text.data() + a_text.size()) {
				return std::nullopt;
			}
			if (a_text.size() == 6) {
				return IM_COL32((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF, 255);
			}
			return IM_COL32((v >> 24) & 0xFF, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
		}

		ImU32* ColorSlot(std::string_view a_key)
		{
			static const std::pair<std::string_view, ImU32*> slots[] = {
				{ "Window", &Color::Window }, { "Panel", &Color::Panel }, { "PanelHover", &Color::PanelHover },
				{ "Row", &Color::Row }, { "RowAlt", &Color::RowAlt }, { "RowHover", &Color::RowHover },
				{ "RowBorder", &Color::RowBorder }, { "Frame", &Color::Frame }, { "FrameHover", &Color::FrameHover },
				{ "FrameActive", &Color::FrameActive }, { "Border", &Color::Border }, { "BorderStrong", &Color::BorderStrong },
				{ "Track", &Color::Track }, { "Text", &Color::Text }, { "TextDim", &Color::TextDim },
				{ "TextFaint", &Color::TextFaint }, { "Value", &Color::Value }, { "Accent", &Color::Gold },
				{ "AccentBright", &Color::GoldBright }, { "AccentDim", &Color::GoldDim }, { "AccentWash", &Color::GoldWash },
				{ "AccentWashStrong", &Color::GoldWashStrong }, { "Danger", &Color::Danger }, { "Ok", &Color::Ok },
				{ "Shadow", &Color::Shadow }, { "PopupBg", &Color::PopupBg }, { "HeaderHover", &Color::HeaderHover },
				{ "ScrollGrab", &Color::ScrollGrab }, { "ScrollGrabHover", &Color::ScrollGrabHover },
				{ "TextSelected", &Color::TextSelected }, { "ModalDim", &Color::ModalDim }, { "TileText", &Color::TileText },
				{ "OnAccent", &Color::OnAccent }, { "KnobHover", &Color::KnobHover },
			};
			for (const auto& [name, slot] : slots) {
				if (name == a_key) {
					return slot;
				}
			}
			if (a_key.size() == 4 && a_key.starts_with("Hue") && a_key[3] >= '0' && a_key[3] <= '9') {
				return &Color::Hues[static_cast<std::size_t>(a_key[3] - '0')];
			}
			return nullptr;
		}
	}

	void LoadTheme()
	{
		constexpr const char* kThemePath = "Data\\SKSE\\Plugins\\RaceMenuAtelier\\theme.ini";
		std::ifstream in(kThemePath);
		if (!in) {
			logger::info("no theme file ({}); Atelier's own colours", kThemePath);
			return;
		}
		std::string line;
		std::string name = "(unnamed)";
		int         applied = 0;
		while (std::getline(in, line)) {
			const auto eq = line.find('=');
			if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[' || eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(std::string_view(line).substr(0, eq));
			const auto value = Trim(std::string_view(line).substr(eq + 1));
			if (key == "sName") {
				name = std::string(value);
			} else if (key == "fRounding") {
				float f = 1.0f;
				if (std::from_chars(value.data(), value.data() + value.size(), f).ec == std::errc{}) {
					RoundingScale = std::clamp(f, 0.0f, 2.0f);
					++applied;
				}
			} else if (auto* slot = ColorSlot(key)) {
				if (const auto color = ParseColor(value)) {
					*slot = *color;
					++applied;
				} else {
					logger::warn("theme: {} = '{}' is not #RRGGBB or #RRGGBBAA", key, value);
				}
			} else {
				logger::warn("theme: unknown key '{}'", key);
			}
		}
		logger::info("theme '{}' loaded from {}: {} value(s)", name, kThemePath, applied);
	}

	float TargetFontSize()
	{
		const auto* io = ImGuiMCP::GetIO();
		const float height = io && io->DisplaySize.y > 0.0f ? io->DisplaySize.y : 1080.0f;
		return std::max(12.0f, height * (21.0f / 1080.0f) * Settings::Get().uiScale);
	}

	void ApplyFont()
	{
		ImGuiMCP::SetWindowFontScale(1.0f);
		const float base = ImGuiMCP::GetFontSize();
		ImGuiMCP::SetWindowFontScale(base > 0.0f ? TargetFontSize() / base : 1.0f);
	}

	void PushStyle()
	{
		using namespace ImGuiMCP;
		const float u = TargetFontSize();

		PushStyleColor(ImGuiCol_WindowBg, Color::Window);
		PushStyleColor(ImGuiCol_ChildBg, Color::Clear);
		PushStyleColor(ImGuiCol_PopupBg, Color::PopupBg);
		PushStyleColor(ImGuiCol_Border, Color::Border);
		PushStyleColor(ImGuiCol_BorderShadow, Color::Clear);
		PushStyleColor(ImGuiCol_FrameBg, Color::Frame);
		PushStyleColor(ImGuiCol_FrameBgHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_FrameBgActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Button, Color::Frame);
		PushStyleColor(ImGuiCol_ButtonHovered, Color::FrameHover);
		PushStyleColor(ImGuiCol_ButtonActive, Color::FrameActive);
		PushStyleColor(ImGuiCol_Header, Color::GoldWash);
		PushStyleColor(ImGuiCol_HeaderHovered, Color::HeaderHover);
		PushStyleColor(ImGuiCol_HeaderActive, Color::GoldWashStrong);
		PushStyleColor(ImGuiCol_Text, Color::Text);
		PushStyleColor(ImGuiCol_TextDisabled, Color::TextFaint);
		PushStyleColor(ImGuiCol_Separator, Color::Border);
		PushStyleColor(ImGuiCol_SliderGrab, Color::Gold);
		PushStyleColor(ImGuiCol_SliderGrabActive, Color::GoldBright);
		PushStyleColor(ImGuiCol_CheckMark, Color::Gold);
		PushStyleColor(ImGuiCol_ScrollbarBg, Color::Clear);
		PushStyleColor(ImGuiCol_ScrollbarGrab, Color::ScrollGrab);
		PushStyleColor(ImGuiCol_ScrollbarGrabHovered, Color::ScrollGrabHover);
		PushStyleColor(ImGuiCol_ScrollbarGrabActive, Color::Gold);
		PushStyleColor(ImGuiCol_TextSelectedBg, Color::TextSelected);
		PushStyleColor(ImGuiCol_NavHighlight, Color::GoldBright);
		PushStyleColor(ImGuiCol_ModalWindowDimBg, Color::ModalDim);
		PushStyleColor(ImGuiCol_ResizeGrip, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripHovered, Color::Clear);
		PushStyleColor(ImGuiCol_ResizeGripActive, Color::Clear);

		PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ u * 0.8f, u * 0.75f });
		PushStyleVar(ImGuiStyleVar_WindowRounding, Round(u * 0.35f));
		PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_ChildRounding, Round(u * 0.3f));
		PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_PopupRounding, Round(u * 0.3f));
		PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
		PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ u * 0.5f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_FrameRounding, Round(u * 0.25f));
		PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ u * 0.45f, u * 0.4f });
		PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2{ u * 0.35f, u * 0.3f });
		PushStyleVar(ImGuiStyleVar_ScrollbarSize, u * 0.55f);
		PushStyleVar(ImGuiStyleVar_ScrollbarRounding, Round(u * 0.3f));
		PushStyleVar(ImGuiStyleVar_GrabMinSize, u * 0.6f);
		PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2{ 0.0f, 0.5f });
		PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2{ 0.5f, 0.5f });
	}

	void PopStyle()
	{
		ImGuiMCP::PopStyleVar(kVars);
		ImGuiMCP::PopStyleColor(kColors);
	}

	void PushIconFont()
	{
		SKSEMenuFramework::PushFont(Icon::Font);
	}

	void PopIconFont()
	{
		FontAwesome::Pop();
	}

	namespace
	{
		struct Picture
		{
			const char* glyph;     // the Icon:: constant it stands for
			const char* file;      // icons\<file>_<size>.png
			const char* fallback;  // drawn when the picture is missing
		};

		// one picture per icon; tools\slice_icons.py writes the files
		const std::vector<Picture>& Pictures()
		{
			static const std::vector<Picture> list{
				{ Icon::Users, "cat_race", Icon::Users }, { Icon::Person, "cat_body", Icon::Person },
				{ Icon::User, "cat_head", Icon::User }, { Icon::Face, "cat_face", Icon::Face },
				{ Icon::Eye, "cat_eyes", Icon::Eye }, { Icon::Brow, "cat_brows", Icon::Brow },
				{ Icon::Mouth, "cat_mouth", Icon::Mouth }, { Icon::Scissors, "cat_hair", Icon::Scissors },
				{ Icon::Palette, "cat_colors", Icon::Palette }, { Icon::Brush, "cat_warpaint", Icon::Brush },
				{ Icon::SprayCan, "cat_bodypaint", Icon::SprayCan }, { Icon::Hand, "cat_handpaint", Icon::Hand },
				{ Icon::Feet, "cat_feetpaint", Icon::Feet }, { Icon::Mask, "cat_facepaint", Icon::Mask },
				{ Icon::Layers, "cat_overlays", Icon::Layers }, { Icon::Sliders, "cat_custom", Icon::Sliders },
				{ Icon::Grid, "cat_all", Icon::Grid },
				{ Icon::Undo, "undo", Icon::Undo }, { Icon::Redo, "redo", Icon::Redo },
				{ Icon::Search, "search", Icon::Search }, { Icon::Filter, "changed", Icon::Filter },
				{ Icon::Columns, "sidepanel", Icon::Columns }, { Icon::Gear, "options", Icon::Gear },
				{ Icon::Check, "done", Icon::Check }, { Icon::Close, "cancel", Icon::Close },
				{ Icon::Reset, "reset", Icon::Reset }, { Icon::Save, "save", Icon::Save },
				{ Icon::Load, "load", Icon::Folder }, { Icon::Folder, "folder", Icon::Folder },
				{ Icon::RotateLeft, "turn_left", Icon::RotateLeft }, { Icon::RotateRight, "turn_right", Icon::RotateRight },
				{ Icon::ZoomIn, "zoom_face", Icon::ZoomIn }, { Icon::ZoomOut, "zoom_body", Icon::ZoomOut },
				{ Icon::Sun, "light_on", Icon::Sun }, { Icon::Moon, "light_off", Icon::Moon },
				{ Icon::Shirt, "undress", Icon::Shirt }, { Icon::Snowflake, "freeze", Icon::Snowflake },
				{ Icon::Scene, "pose_play", Icon::Person }, { Icon::Stop, "pose_stop", Icon::Stop },
				{ Icon::Mars, "male", Icon::Mars }, { Icon::Venus, "female", Icon::Venus },
				{ Icon::Info, "details", Icon::Info }, { Icon::Export, "bodyslide", Icon::Export },
				{ Icon::Wrench, "racemenu", Icon::Wrench }, { Icon::Wand, "sculpt", Icon::Wand },
				{ Icon::Bug, "diagnostic", Icon::Bug }, { Icon::Refresh, "reread", Icon::Refresh },
				{ Icon::Emblem, "emblem", Icon::Feather }, { Icon::Gem, "gem", Icon::Dot },
			};
			return list;
		}

		const Picture* FindPicture(const char* a_icon)
		{
			if (!a_icon) {
				return nullptr;
			}
			for (const auto& p : Pictures()) {
				if (std::strcmp(p.glyph, a_icon) == 0) {
					return &p;
				}
			}
			return nullptr;
		}

		// the smallest written size that is not smaller than the drawn one, so the picture is only ever shrunk
		int PictureSize(float a_px)
		{
			for (const int size : { 24, 32, 48, 64, 96 }) {
				if (static_cast<float>(size) >= a_px) {
					return size;
				}
			}
			return 96;
		}

		// loaded on first use through the framework (WIC reads the PNG from the virtual Data folder); a file
		// that fails to load is remembered as missing and the glyph is drawn instead
		ImGuiMCP::ImTextureID PictureTexture(const Picture& a_picture, float a_px)
		{
			static std::unordered_map<std::string, ImGuiMCP::ImTextureID> cache;
			const auto key = std::format("{}_{}", a_picture.file, PictureSize(a_px));
			auto       it = cache.find(key);
			if (it == cache.end()) {
				const auto path = std::format("Data/Interface/RaceMenuAtelier/icons/{}.png", key);
				auto       texture = SKSEMenuFramework::LoadTexture(path);
				if (!texture) {
					logger::warn("icon {} could not be loaded; the font glyph is used", path);
				}
				it = cache.emplace(key, texture).first;
			}
			return it->second;
		}

		// pictures take the square the icon font gives a glyph, a little larger since they have no side bearing
		float IconPixels()
		{
			PushIconFont();
			const float h = ImGuiMCP::GetTextLineHeight();
			PopIconFont();
			return std::round(h * 1.1f);
		}

		void DrawGlyph(ImGuiMCP::ImDrawList* a_list, ImVec2 a_center, const char* a_icon, ImU32 a_color)
		{
			PushIconFont();
			const auto size = ImGuiMCP::CalcTextSize(a_icon);
			ImGuiMCP::ImDrawListManager::AddText(a_list, { std::floor(a_center.x - size.x * 0.5f), std::floor(a_center.y - size.y * 0.5f) }, a_color, a_icon);
			PopIconFont();
		}
	}

	ImVec2 IconSize(const char* a_icon)
	{
		if (const auto* picture = FindPicture(a_icon)) {
			const float px = IconPixels();
			if (PictureTexture(*picture, px)) {
				return { px, px };
			}
			a_icon = picture->fallback;
		}
		PushIconFont();
		const auto size = ImGuiMCP::CalcTextSize(a_icon);
		PopIconFont();
		return size;
	}

	void DrawIconSized(ImGuiMCP::ImDrawList* a_list, ImVec2 a_center, const char* a_icon, ImU32 a_color, float a_size)
	{
		if (const auto* picture = FindPicture(a_icon)) {
			if (const auto texture = PictureTexture(*picture, a_size)) {
				const ImVec2 min{ std::round(a_center.x - a_size * 0.5f), std::round(a_center.y - a_size * 0.5f) };
				ImGuiMCP::ImDrawListManager::AddImage(a_list, texture, min, { min.x + a_size, min.y + a_size }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, a_color);
				return;
			}
			a_icon = picture->fallback;
		}
		DrawGlyph(a_list, a_center, a_icon, a_color);
	}

	void DrawIcon(ImGuiMCP::ImDrawList* a_list, ImVec2 a_center, const char* a_icon, ImU32 a_color)
	{
		DrawIconSized(a_list, a_center, a_icon, a_color, IconPixels());
	}
}
