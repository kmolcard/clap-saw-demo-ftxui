//
// Created by Paul Walker on 6/10/22.
//

#ifndef CLAP_SAW_DEMO_EDITOR_H
#define CLAP_SAW_DEMO_EDITOR_H
#include "clap-saw-demo.h"
#include "ftxui-clap-support/ftxui-clap-editor.h"
#include <unordered_map>
#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace sst::clap_saw_demo
{

struct ClapSawDemoEditor : public ftxui_clap_editor
{
    ClapSawDemoEditor(ClapSawDemo::SynthToUI_Queue_t &, ClapSawDemo::UIToSynth_Queue_t &,
                      const ClapSawDemo::DataCopyForUI &, std::function<void()>);

    // FTXUI component creation
    ftxui::Component onCreateComponent() override;

    // Lifecycle callbacks
    void onGuiCreate() override;
    void onGuiDestroy() override;
    void onParameterUpdate() override;

    // GUI Helper functions for FTXUI

    // create a slider component for a parameter
    ftxui::Component createSliderForParam(clap_id pid, const std::string &label, float min,
                                          float max);
    // a on/off switch component for a parameter
    ftxui::Component createSwitchForParam(clap_id pid, const std::string &label,
                                          bool reverse = false);
    // radio button component for parameter selection
    ftxui::Component createRadioButtonForParam(clap_id pid,
                                               std::vector<std::pair<int, std::string>> options);

    // Component initialization
    void createParameterComponents();

    // UI element creators
    ftxui::Element renderHeader();
    ftxui::Element renderOscillatorSection();
    ftxui::Element renderAmplifierSection();
    ftxui::Element renderFilterSection();
    ftxui::Element renderFooter();

    // Parameter Queues

    // queues for parameter updates from UI to DSP and other way round
    ClapSawDemo::SynthToUI_Queue_t &inbound;
    ClapSawDemo::UIToSynth_Queue_t &outbound;
    const ClapSawDemo::DataCopyForUI &synthData;
    std::function<void()> paramRequestFlush;

    // update the parameter state for UI, has to be called each frame
    void dequeueParamUpdates();

    // state copy of parameter values/edit state for UI
    std::unordered_map<clap_id, double> paramCopy;
    std::unordered_map<clap_id, bool> paramInEdit;

    // FTXUI components
    ftxui::Component main_container_;
    std::unordered_map<clap_id, ftxui::Component> param_components_;

    // UI state
    int selected_tab_ = 0;
    std::vector<std::string> tab_entries_;
    std::string status_message_ = "Ready";
};

} // namespace sst::clap_saw_demo
#endif
