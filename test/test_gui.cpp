#include <iostream>
#include <dlfcn.h>
#include <clap/clap.h>
#include <unistd.h>

// Simple test host callbacks
static void host_log(const clap_host *host, clap_log_severity severity, const char *msg)
{
    std::cout << "[HOST LOG] " << msg << std::endl;
}

// Simple host implementation
static const clap_host test_host = {
    CLAP_VERSION,
    nullptr, // host_data
    "Test Host",  "Test", "http://test.com", "1.0.0",
    nullptr, // get_extension
    nullptr, // request_restart
    nullptr, // request_process
    nullptr, // request_callback
};

int main(int argc, char *argv[])
{
    std::cout << "Starting GUI test..." << std::endl;

    const char *plugin_path = "clap-saw-demo-ftxui.clap/Contents/MacOS/clap-saw-demo-ftxui";
    if (argc > 1)
    {
        plugin_path = argv[1];
    }

    // Load the plugin
    void *handle = dlopen(plugin_path, RTLD_LAZY);
    if (!handle)
    {
        std::cerr << "Cannot load plugin from " << plugin_path << ": " << dlerror() << std::endl;
        return 1;
    }
    std::cout << "Plugin loaded successfully" << std::endl;

    // Get the entry point
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(handle, "clap_entry");
    if (!entry)
    {
        std::cerr << "Cannot find clap_entry symbol: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }

    // Initialize the plugin
    if (!entry->init("/tmp"))
    {
        std::cerr << "Failed to initialize plugin" << std::endl;
        dlclose(handle);
        return 1;
    }

    // Get plugin factory
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory)
    {
        std::cerr << "Cannot get plugin factory" << std::endl;
        entry->deinit();
        dlclose(handle);
        return 1;
    }

    // Create plugin instance
    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    if (!desc)
    {
        std::cerr << "Cannot get plugin descriptor" << std::endl;
        entry->deinit();
        dlclose(handle);
        return 1;
    }

    const clap_plugin_t *plugin = factory->create_plugin(factory, &test_host, desc->id);
    if (!plugin)
    {
        std::cerr << "Cannot create plugin instance" << std::endl;
        entry->deinit();
        dlclose(handle);
        return 1;
    }
    std::cout << "Plugin instance created successfully" << std::endl;

    // Initialize the plugin instance
    if (!plugin->init(plugin))
    {
        std::cerr << "Failed to initialize plugin instance" << std::endl;
        plugin->destroy(plugin);
        entry->deinit();
        dlclose(handle);
        return 1;
    }
    std::cout << "Plugin instance initialized" << std::endl;

    // Test GUI extension
    const clap_plugin_gui_t *gui =
        (const clap_plugin_gui_t *)plugin->get_extension(plugin, CLAP_EXT_GUI);
    if (!gui)
    {
        std::cout << "Plugin does not support GUI extension" << std::endl;
    }
    else
    {
        std::cout << "GUI extension found!" << std::endl;

        // Test if API is supported
        bool api_supported = gui->is_api_supported(plugin, CLAP_WINDOW_API_COCOA, false);
        std::cout << "Cocoa API supported: " << (api_supported ? "yes" : "no") << std::endl;

        // Try to create GUI
        if (gui->create(plugin, CLAP_WINDOW_API_COCOA, false))
        {
            std::cout << "GUI created successfully!" << std::endl;

            // Get preferred size
            uint32_t width, height;
            if (gui->get_size(plugin, &width, &height))
            {
                std::cout << "GUI preferred size: " << width << "x" << height << std::endl;
            }

            // Test if GUI can resize
            bool can_resize = gui->can_resize(plugin);
            std::cout << "GUI can resize: " << (can_resize ? "yes" : "no") << std::endl;

            // Show GUI for a brief moment
            if (gui->show(plugin))
            {
                std::cout << "GUI shown successfully!" << std::endl;
                std::cout << "Waiting 3 seconds to test GUI display..." << std::endl;
                sleep(3);

                gui->hide(plugin);
                std::cout << "GUI hidden" << std::endl;
            }
            else
            {
                std::cout << "Failed to show GUI" << std::endl;
            }

            gui->destroy(plugin);
            std::cout << "GUI destroyed" << std::endl;
        }
        else
        {
            std::cout << "Failed to create GUI" << std::endl;
        }
    }

    // Clean up
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(handle);

    std::cout << "GUI test completed successfully!" << std::endl;
    return 0;
}
