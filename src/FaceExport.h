#pragma once

// "FaceGen for NPC": writes the player's current face as the FaceGen head of
// another NPC record, plus the record data a separate xEdit script applies.
//
//   Data\meshes\actors\character\FaceGenData\FaceGeom\<plugin>\<id>.nif
//   Data\textures\actors\character\FaceGenData\FaceTint\<plugin>\<id>.dds
//   Data\SKSE\Plugins\RaceMenuAtelier\NPCFaces\<plugin>\<id>.json
//
// Under MO2 all three land in Overwrite.

namespace RMA::FaceExport
{
	using StatusFn = std::function<void(std::string)>;

	// a_query: an EditorID, "Plugin.esp|D62" / "D62:Plugin.esp", or a runtime
	// FormID as the console prints it ("FE17ED62"). Runs on the game thread
	// after a short wait for a neutral face; reports through a_status.
	void Start(std::string a_query, StatusFn a_status);

	[[nodiscard]] bool IsBusy();

	// The NPC picker: every NPC record defined by a loaded plugin, read once on
	// the game thread. NPCList() is empty until RequestNPCList() has finished.
	struct NPCEntry
	{
		std::string plugin;    // defining plugin, the FaceGen folder name
		std::string formID;    // local FormID, 8 hex digits as FaceGen names it
		std::string name;      // in-game name, may be empty
		std::string editorID;  // usually empty: the game keeps few NPC EditorIDs
		std::string search;    // lower-case name, EditorID, FormID and plugin
		bool        female{ false };
		bool        templated{ false };  // takes its face from a template

		[[nodiscard]] std::string Query() const { return plugin + "|" + formID; }
	};

	void RequestNPCList();
	[[nodiscard]] std::shared_ptr<const std::vector<NPCEntry>> NPCList();
	[[nodiscard]] bool NPCListPending();
}
