//
// Created by Paul Walker on 6/10/22.
//

#include "clap-saw-demo-editor.h"
#include "clap-saw-demo.h"
#include "clap/clap.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>

#include <clap/helpers/host-proxy.hxx>

#define STR_INDIR(x) #x
#define STR(x) STR_INDIR(x)

using namespace ftxui;

namespace sst::clap_saw_demo
{
/*
 * Part one of this implementation is the plugin adapters which allow a GUI to attach.
 * These are the ClapSawDemo methods. First up: Which windowing API do we support.
 * Pretty obviously, mac supports cocoa, windows supports win32, and linux supports
 * X11.
 */
bool ClapSawDemo::guiIsApiSupported(const char *api, bool isFloating) noexcept
{
    if (isFloating)
        return false;
#if IS_MAC
    if (strcmp(api, CLAP_WINDOW_API_COCOA) == 0)
        return true;
#endif

#if IS_WIN
    if (strcmp(api, CLAP_WINDOW_API_WIN32) == 0)
        return true;
#endif

#if IS_LINUX
    if (strcmp(api, CLAP_WINDOW_API_X11) == 0)
        return true;
#endif

    return false;
}

/*
 * GUICreate gets called when the host requests the plugin create its editor with
 * a given API. We ignore the API and isFloating here, because we handled them
 * above and assume our host follows the protocol that it only calls us with
 * values which are supported.
 */
bool ClapSawDemo::guiCreate(const char *api, bool isFloating) noexcept
{
    _DBGMARK;
    assert(!editor);
    editor =
        new ClapSawDemoEditor(toUiQ, fromUiQ, dataCopyForUI, [this]() { editorParamsFlush(); });
    const clap_host_timer_support_t *timer{nullptr};
    _host.getExtension(timer, CLAP_EXT_TIMER_SUPPORT);
    return ftxui_clap_guiCreateWith(editor, timer);
}

/*
 * guiDestroy destroys the editor object and returns it to the
 * nullptr sentinel, to stop ::process sending events to the ui.
 */
void ClapSawDemo::guiDestroy() noexcept
{
    assert(editor);
    const clap_host_timer_support_t *timer{nullptr};
    _host.getExtension(timer, CLAP_EXT_TIMER_SUPPORT);
    ftxui_clap_guiDestroyWith(editor, timer);
    delete editor;
    editor = nullptr;
}

/*
 * guiSetParent is the core API for a clap HOST which has a window to
 * reparent the editor to that host managed window. It sends a
 * `const clap_window *window` data structure which contains a union of
 * platform specific window pointers.
 */
bool ClapSawDemo::guiSetParent(const clap_window *window) noexcept
{
    assert(editor);
    auto res = ftxui_clap_guiSetParentWith(editor, window);
    if (!res)
        return false;

    if (dataCopyForUI.isProcessing)
    {
        // and ask the engine to refresh from the processing thread
        refreshUIValues = true;
    }
    else
    {
        // Pull the parameters on the main thread
        for (const auto &[k, v] : paramToValue)
        {
            auto r = ToUI();
            r.type = ToUI::PARAM_VALUE;
            r.id = k;
            r.value = *v;
            toUiQ.try_enqueue(r);
        }
    }
    // And we are done!
    return true;
}

/*
 * guiSetScale is the core API that allows the Host to set the absolute GUI
 * scaling factor, and override any OS info. This is important to allow the UI
 * to correctly reflect what has been specified by the Host and not have to
 * work out the users intentions through some sort of magic.
 *
 * Obviously, the value will depend on how the host chooses to implement it.
 * The value is normalised, with 1.0 representing 100% scaling.
 */
bool ClapSawDemo::guiSetScale(double scale) noexcept
{
    _DBGCOUT << _D(scale) << std::endl;
    return false;
}

/*
 * Sizing is described in the gui extension, but this implementation
 * means that if the host drags to resize, we accept its size and resize our frame
 */
bool ClapSawDemo::guiSetSize(uint32_t width, uint32_t height) noexcept
{
    _DBGCOUT << _D(width) << _D(height) << std::endl;
    assert(editor);
    return ftxui_clap_guiSetSizeWith(editor, width, height);
}

/*
 * Returns the size of the UI window, presumable so a host can better layout plugin UIs
 * if grouped together.
 */
bool ClapSawDemo::guiGetSize(uint32_t *width, uint32_t *height) noexcept
{
    *width = 540;
    *height = 324;
    return true;
}

bool ClapSawDemo::guiAdjustSize(uint32_t *width, uint32_t *height) noexcept { return true; }

/*
 * guiShow shows the GUI window and makes it visible to the user.
 * This is called by the host when it wants to display the plugin GUI.
 */
bool ClapSawDemo::guiShow() noexcept
{
    assert(editor);
    printf("ClapSawDemo::guiShow() called\n");
    bool result = ftxui_clap_guiShowWith(editor);
    printf("ftxui_clap_guiShowWith returned: %s\n", result ? "true" : "false");
    return result;
}

/*
 * guiHide hides the GUI window but doesn't destroy it.
 * This is called by the host when it wants to hide the plugin GUI.
 */
bool ClapSawDemo::guiHide() noexcept
{
    assert(editor);
    return ftxui_clap_guiHideWith(editor);
}

ClapSawDemoEditor::ClapSawDemoEditor(ClapSawDemo::SynthToUI_Queue_t &i,
                                     ClapSawDemo::UIToSynth_Queue_t &o,
                                     const ClapSawDemo::DataCopyForUI &d, std::function<void()> pf)
    : inbound(i), outbound(o), synthData(d), paramRequestFlush(std::move(pf))
{
}

ftxui::Component ClapSawDemoEditor::createSliderForParam(clap_id pid, const std::string &label,
                                                         float min, float max)
{
    // Create a slider that updates parameter values
    auto slider_value = std::make_shared<float>(paramCopy[pid]);

    auto slider = ftxui::Slider(label, slider_value.get(), min, max);

    // Wrap the slider to handle parameter updates
    auto wrapped_slider = ftxui::Container::Horizontal(
        {ftxui::Renderer(
             [label] { return ftxui::text(label) | ftxui::size(ftxui::WIDTH, ftxui::EQUAL, 15); }),
         slider});

    // Add event handling for parameter changes
    auto event_handler =
        ftxui::CatchEvent(wrapped_slider,
                          [this, pid, slider_value, min, max](ftxui::Event event)
                          {
                              if (event.is_mouse())
                              {
                                  // Handle mouse events to detect start/end of editing
                                  if (event.mouse().button == ftxui::Mouse::Left)
                                  {
                                      if (event.mouse().motion == ftxui::Mouse::Pressed)
                                      {
                                          if (!paramInEdit[pid])
                                          {
                                              paramInEdit[pid] = true;
                                              auto q = ClapSawDemo::FromUI();
                                              q.id = pid;
                                              q.type = ClapSawDemo::FromUI::MType::BEGIN_EDIT;
                                              q.value = *slider_value;
                                              outbound.try_enqueue(q);
                                          }
                                      }
                                      else if (event.mouse().motion == ftxui::Mouse::Released)
                                      {
                                          if (paramInEdit[pid])
                                          {
                                              paramInEdit[pid] = false;
                                              auto q = ClapSawDemo::FromUI();
                                              q.id = pid;
                                              q.type = ClapSawDemo::FromUI::MType::END_EDIT;
                                              q.value = *slider_value;
                                              outbound.try_enqueue(q);
                                          }
                                      }
                                  }

                                  // Check if value changed and send update
                                  if (*slider_value != paramCopy[pid])
                                  {
                                      auto q = ClapSawDemo::FromUI();
                                      q.id = pid;
                                      q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
                                      q.value = *slider_value;
                                      outbound.try_enqueue(q);
                                      paramCopy[pid] = *slider_value;
                                  }
                              }
                              return false; // Don't consume the event
                          });

    // Store the component for later reference
    param_components_[pid] = event_handler;

    return event_handler | ftxui::border;
}

ftxui::Component ClapSawDemoEditor::createSwitchForParam(clap_id pid, const std::string &label,
                                                         bool reverse)
{
    // Create a checkbox/toggle that updates parameter values
    auto checkbox_state =
        std::make_shared<bool>(reverse ? (paramCopy[pid] < 0.5f) : (paramCopy[pid] > 0.5f));

    auto checkbox = ftxui::Checkbox(label, checkbox_state.get());

    // Add event handling for parameter changes
    auto event_handler =
        ftxui::CatchEvent(checkbox,
                          [this, pid, checkbox_state, reverse](ftxui::Event event)
                          {
                              if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left &&
                                  event.mouse().motion == ftxui::Mouse::Released)
                              {
                                  // Send parameter update
                                  auto q = ClapSawDemo::FromUI();
                                  q.id = pid;
                                  q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
                                  if (reverse)
                                  {
                                      q.value = *checkbox_state ? 0.f : 1.f;
                                  }
                                  else
                                  {
                                      q.value = *checkbox_state ? 1.f : 0.f;
                                  }
                                  outbound.try_enqueue(q);
                                  paramCopy[pid] = q.value;
                              }
                              return false; // Don't consume the event
                          });

    // Store the component for later reference
    param_components_[pid] = event_handler;

    return event_handler | ftxui::border;
}

ftxui::Component
ClapSawDemoEditor::createRadioButtonForParam(clap_id pid,
                                             std::vector<std::pair<int, std::string>> options)
{
    // Create radio buttons for parameter selection
    auto selected = std::make_shared<int>(static_cast<int>(paramCopy[pid]));

    std::vector<std::string> option_labels;
    for (const auto &option : options)
    {
        option_labels.push_back(option.second);
    }

    auto radiobox = ftxui::Radiobox(&option_labels, selected.get());

    // Add event handling for parameter changes
    auto event_handler =
        ftxui::CatchEvent(radiobox,
                          [this, pid, selected, options](ftxui::Event event)
                          {
                              if (event.is_mouse() && event.mouse().button == ftxui::Mouse::Left &&
                                  event.mouse().motion == ftxui::Mouse::Released)
                              {
                                  // Find the corresponding option value
                                  int option_value = *selected;
                                  if (*selected < options.size())
                                  {
                                      option_value = options[*selected].first;
                                  }

                                  if (option_value != static_cast<int>(paramCopy[pid]))
                                  {
                                      // Send parameter update
                                      auto q = ClapSawDemo::FromUI();
                                      q.id = pid;
                                      q.type = ClapSawDemo::FromUI::MType::ADJUST_VALUE;
                                      q.value = option_value;
                                      outbound.try_enqueue(q);
                                      paramCopy[pid] = option_value;
                                  }
                              }
                              return false; // Don't consume the event
                          });

    // Store the component for later reference
    param_components_[pid] = event_handler;

    return event_handler | ftxui::border;
}

void ClapSawDemoEditor::dequeueParamUpdates()
{
    ClapSawDemo::ToUI r;
    while (inbound.try_dequeue(r))
    {
        if (r.type == ClapSawDemo::ToUI::MType::PARAM_VALUE)
        {
            paramCopy[r.id] = r.value;
            paramInEdit[r.id] = false;
        }
    }
}

ftxui::Component ClapSawDemoEditor::onCreateComponent()
{
    // Update UI parameters first
    dequeueParamUpdates();

    // Create all parameter components once
    createParameterComponents();

    // Create main container with tab navigation
    tab_entries_ = {"Oscillator", "Filter", "Amplifier"};
    auto tabs = ftxui::Menu(&tab_entries_, &selected_tab_);

    // Create section containers that hold actual components
    auto oscillator_container =
        ftxui::Container::Vertical({param_components_[ClapSawDemo::pmUnisonCount],
                                    param_components_[ClapSawDemo::pmUnisonSpread],
                                    param_components_[ClapSawDemo::pmOscDetune]});

    auto filter_container = ftxui::Container::Vertical(
        {param_components_[ClapSawDemo::pmPreFilterVCA], param_components_[ClapSawDemo::pmCutoff],
         param_components_[ClapSawDemo::pmResonance],
         param_components_[ClapSawDemo::pmFilterMode]});

    auto amplifier_container = ftxui::Container::Vertical(
        {param_components_[ClapSawDemo::pmAmpAttack], param_components_[ClapSawDemo::pmAmpRelease],
         param_components_[ClapSawDemo::pmAmpIsGate]});

    // Create main content area that shows the right section
    auto content = ftxui::Container::Tab(
        {oscillator_container, filter_container, amplifier_container}, &selected_tab_);

    // Wrap with renderer to add section styling
    auto styled_content = ftxui::Renderer(content,
                                          [this]
                                          {
                                              dequeueParamUpdates(); // Keep UI in sync

                                              switch (selected_tab_)
                                              {
                                              case 0:
                                                  return renderOscillatorSection();
                                              case 1:
                                                  return renderFilterSection();
                                              case 2:
                                                  return renderAmplifierSection();
                                              default:
                                                  return renderOscillatorSection();
                                              }
                                          });

    // Main container with header and footer
    auto main_container = ftxui::Container::Vertical(
        {ftxui::Renderer(
             [] { return ftxui::text("CLAP Saw Demo - FTXUI") | ftxui::bold | ftxui::center; }),
         ftxui::Renderer([] { return ftxui::separator(); }), tabs,
         ftxui::Renderer([] { return ftxui::separator(); }), styled_content,
         ftxui::Renderer(
             [this]
             {
                 return ftxui::hbox(
                            {ftxui::text("Polyphony: " + std::to_string(synthData.polyphony)) |
                                 ftxui::flex,
                             ftxui::text("Status: " + status_message_)}) |
                        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1);
             })});

    return main_container | ftxui::border;
}

void ClapSawDemoEditor::onGuiCreate()
{
    // Initialize any GUI-specific state
}

void ClapSawDemoEditor::onGuiDestroy()
{
    // Clean up GUI resources
    param_components_.clear();
}

void ClapSawDemoEditor::onParameterUpdate() { dequeueParamUpdates(); }

void ClapSawDemoEditor::createParameterComponents()
{
    // Clear existing components
    param_components_.clear();

    // Create oscillator components
    param_components_[ClapSawDemo::pmUnisonCount] =
        createSliderForParam(ClapSawDemo::pmUnisonCount, "Unison Count", 1, 7);
    param_components_[ClapSawDemo::pmUnisonSpread] =
        createSliderForParam(ClapSawDemo::pmUnisonSpread, "Spread (cents)", 0, 100);
    param_components_[ClapSawDemo::pmOscDetune] =
        createSliderForParam(ClapSawDemo::pmOscDetune, "Detune (cents)", -200, 200);

    // Create filter components
    param_components_[ClapSawDemo::pmPreFilterVCA] =
        createSliderForParam(ClapSawDemo::pmPreFilterVCA, "Pre-Filter VCA", 0.0f, 1.0f);
    param_components_[ClapSawDemo::pmCutoff] =
        createSliderForParam(ClapSawDemo::pmCutoff, "Cutoff (keys)", 1, 127);
    param_components_[ClapSawDemo::pmResonance] =
        createSliderForParam(ClapSawDemo::pmResonance, "Resonance", 0.0f, 1.0f);

    // Filter mode options
    std::vector<std::pair<int, std::string>> filter_modes = {
        {0, "LP (Low Pass)"}, {1, "HP (High Pass)"}, {2, "BP (Band Pass)"},
        {3, "NOTCH"},         {4, "PEAK"},           {5, "ALL"}};
    param_components_[ClapSawDemo::pmFilterMode] =
        createRadioButtonForParam(ClapSawDemo::pmFilterMode, filter_modes);

    // Create amplifier components
    param_components_[ClapSawDemo::pmAmpAttack] =
        createSliderForParam(ClapSawDemo::pmAmpAttack, "Attack (s)", 0.0f, 1.0f);
    param_components_[ClapSawDemo::pmAmpRelease] =
        createSliderForParam(ClapSawDemo::pmAmpRelease, "Release (s)", 0.0f, 1.0f);
    param_components_[ClapSawDemo::pmAmpIsGate] =
        createSwitchForParam(ClapSawDemo::pmAmpIsGate, "Deactivate Envelope", false);
}

// Section renderer implementations
ftxui::Element ClapSawDemoEditor::renderOscillatorSection()
{
    // Create layout with parameter value displays
    return ftxui::vbox({
               ftxui::text("OSCILLATOR") | ftxui::bold | ftxui::center, ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox(
                        {ftxui::text("Unison Count: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmUnisonCount])),
                         ftxui::text("Current: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmUnisonCount]))}) |
                        ftxui::flex,
                    ftxui::separator(),
                    ftxui::vbox(
                        {ftxui::text("Spread: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmUnisonSpread]) +
                                     " cents"),
                         ftxui::text(
                             "Current: " +
                             std::to_string((int)paramCopy[ClapSawDemo::pmUnisonSpread]))}) |
                        ftxui::flex}),
               ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox(
                        {ftxui::text(
                             "Detune: " + std::to_string((int)paramCopy[ClapSawDemo::pmOscDetune]) +
                             " cents"),
                         ftxui::text("Current: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmOscDetune]))}) |
                    ftxui::flex}),
               ftxui::text("") // spacing
           }) |
           ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 12);
}

ftxui::Element ClapSawDemoEditor::renderFilterSection()
{
    // Create layout with parameter value displays
    return ftxui::vbox({
               ftxui::text("FILTER") | ftxui::bold | ftxui::center, ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox(
                        {ftxui::text(
                             "Pre-Filter VCA: " +
                             std::to_string((int)(paramCopy[ClapSawDemo::pmPreFilterVCA] * 100)) +
                             "%"),
                         ftxui::text(
                             "Current: " +
                             std::to_string((int)(paramCopy[ClapSawDemo::pmPreFilterVCA] * 100)) +
                             "%")}) |
                        ftxui::flex,
                    ftxui::separator(),
                    ftxui::vbox(
                        {ftxui::text(
                             "Cutoff: " + std::to_string((int)paramCopy[ClapSawDemo::pmCutoff]) +
                             " keys"),
                         ftxui::text("Current: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmCutoff]))}) |
                        ftxui::flex}),
               ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox({ftxui::text("Resonance: " +
                                             std::to_string(
                                                 (int)(paramCopy[ClapSawDemo::pmResonance] * 100)) +
                                             "%"),
                                 ftxui::text("Current: " +
                                             std::to_string(
                                                 (int)(paramCopy[ClapSawDemo::pmResonance] * 100)) +
                                             "%")}) |
                        ftxui::flex,
                    ftxui::separator(),
                    ftxui::vbox(
                        {ftxui::text("Filter Type:"),
                         ftxui::text("Mode: " +
                                     std::to_string((int)paramCopy[ClapSawDemo::pmFilterMode]))}) |
                        ftxui::flex}),
               ftxui::text("") // spacing
           }) |
           ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 12);
}

ftxui::Element ClapSawDemoEditor::renderAmplifierSection()
{
    // Create layout with parameter value displays
    return ftxui::vbox({
               ftxui::text("AMPLIFIER") | ftxui::bold | ftxui::center, ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox({ftxui::text("Attack: " +
                                             std::to_string((
                                                 int)(paramCopy[ClapSawDemo::pmAmpAttack] * 1000)) +
                                             " ms"),
                                 ftxui::text("Current: " +
                                             std::to_string((
                                                 int)(paramCopy[ClapSawDemo::pmAmpAttack] * 1000)) +
                                             " ms")}) |
                        ftxui::flex,
                    ftxui::separator(),
                    ftxui::vbox(
                        {ftxui::text(
                             "Release: " +
                             std::to_string((int)(paramCopy[ClapSawDemo::pmAmpRelease] * 1000)) +
                             " ms"),
                         ftxui::text(
                             "Current: " +
                             std::to_string((int)(paramCopy[ClapSawDemo::pmAmpRelease] * 1000)) +
                             " ms")}) |
                        ftxui::flex}),
               ftxui::separator(),
               ftxui::hbox(
                   {ftxui::vbox({ftxui::text("Gate Mode: " +
                                             std::string(paramCopy[ClapSawDemo::pmAmpIsGate] > 0.5f
                                                             ? "ON"
                                                             : "OFF")),
                                 ftxui::text("Envelope: " +
                                             std::string(paramCopy[ClapSawDemo::pmAmpIsGate] > 0.5f
                                                             ? "Disabled"
                                                             : "Enabled"))}) |
                    ftxui::flex}),
               ftxui::text("") // spacing
           }) |
           ftxui::border | ftxui::size(ftxui::HEIGHT, ftxui::GREATER_THAN, 12);
}

} // namespace sst::clap_saw_demo
