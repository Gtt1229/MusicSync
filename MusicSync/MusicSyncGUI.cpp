#include "pch.h"
#include "MusicSync.h"

std::string MusicSync::GetPluginName()
{
	return "MusicSync";
}

void MusicSync::SetImGuiContext(uintptr_t ctx)
{
	ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

std::string MusicSync::GetMenuName()
{
	return "MusicSync";
}

std::string MusicSync::GetMenuTitle()
{
	return menuTitle_;
}

void MusicSync::RenderSettings() {
    // Get CVars
    CVarWrapper enableCvar = cvarManager->getCvar("music_overlay_enabled");
    CVarWrapper coverEnableCvar = cvarManager->getCvar("music_overlay_show_cover");
    CVarWrapper scaleCvar = cvarManager->getCvar("music_overlay_scale");
    CVarWrapper xposCvar = cvarManager->getCvar("music_overlay_x");
    CVarWrapper yposCvar = cvarManager->getCvar("music_overlay_y");
    CVarWrapper textColorCvar = cvarManager->getCvar("music_overlay_text_color");
    CVarWrapper bkgColorCvar = cvarManager->getCvar("music_overlay_background_color");
    CVarWrapper bkgOpacityCvar = cvarManager->getCvar("music_overlay_background_opacity");
	CVarWrapper alwaysEnabledCvar = cvarManager->getCvar("music_overlay_always_enabled");

    if (!enableCvar || !coverEnableCvar || !scaleCvar || !xposCvar || !yposCvar || 
        !textColorCvar || !bkgColorCvar || !bkgOpacityCvar || !alwaysEnabledCvar) { 
        return; 
    }

    LinearColor bkgColor = bkgColorCvar.getColorValue()/255;
    LinearColor textColor = textColorCvar.getColorValue()/255;
    int bkgOpacity = bkgOpacityCvar.getIntValue();
    bool alwaysEnabled = alwaysEnabledCvar.getBoolValue();
    bool enabled = enableCvar.getBoolValue();
    bool coverEnabled = coverEnableCvar.getBoolValue();
    float scale = scaleCvar.getFloatValue();
    float xpos = xposCvar.getFloatValue();  // Now percentage (0-100)
    float ypos = yposCvar.getFloatValue();  // Now percentage (0-100)

    // Get current display size
    ImGuiIO& io = ImGui::GetIO();
    float screenWidth = io.DisplaySize.x;
    float screenHeight = io.DisplaySize.y;

    if (ImGui::Checkbox("Enable MusicSync", &enabled)) {
        enableCvar.setValue(enabled);
    }
    if (ImGui::Checkbox("Show Album Cover", &coverEnabled)) {
        coverEnableCvar.setValue(coverEnabled);
    }
    if (ImGui::Checkbox("Always render", &alwaysEnabled)) {
        if (alwaysEnabled) {
            isScoreboardVisible = true;
			alwaysEnabledCvar.setValue(alwaysEnabled);
		}
        else {
            isScoreboardVisible = false;
            alwaysEnabledCvar.setValue(alwaysEnabled);
        }
    }
      

    if (ImGui::SliderFloat("Overlay Scale", &scale, 0.5f, 2.0f)) {
        scaleCvar.setValue(scale);
    }
    
    // Position sliders
    if (ImGui::SliderFloat("Overlay X Position (%)", &xpos, 0.0f, 100.0f, "%.1f%%")) {
        xposCvar.setValue(xpos);
    }
    ImGui::SameLine();
    ImGui::Text("(%d px)", static_cast<int>((xpos / 100.0f) * screenWidth));
    
    if (ImGui::SliderFloat("Overlay Y Position (%)", &ypos, 0.0f, 100.0f, "%.1f%%")) {
        yposCvar.setValue(ypos);
    }
    ImGui::SameLine();
    ImGui::Text("(%d px)", static_cast<int>((ypos / 100.0f) * screenHeight));

    if (ImGui::ColorEdit4("Text Color", &textColor.R, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        textColorCvar.setValue(textColor * 255);
    }
    if (ImGui::ColorEdit4("Background color", &bkgColor.R, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
        bkgColorCvar.setValue(bkgColor*255);
    }
    if (ImGui::SliderInt("Background Opacity", &bkgOpacity, 0, 255)) {
        bkgOpacityCvar.setValue(bkgOpacity);
    };
}

bool MusicSync::ShouldBlockInput()
{
	return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

bool MusicSync::IsActiveOverlay()
{
	return true;
}

void MusicSync::OnOpen()
{
	isWindowOpen_ = true;
}

void MusicSync::OnClose()
{
	isWindowOpen_ = false;
}

void MusicSync::Render()
{

	if (!ImGui::Begin(menuTitle_.c_str(), &isWindowOpen_, ImGuiWindowFlags_None))
	{
		// Early out if the window is collapsed, as an optimization.
		ImGui::End();
		return;
	}

	ImGui::End();

	if (!isWindowOpen_)
	{
		_globalCvarManager->executeCommand("togglemenu " + GetMenuName());
	}
}