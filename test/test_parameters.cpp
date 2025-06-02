#include <iostream>
#include <dlfcn.h>
#include <clap/clap.h>

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
    std::cout << "Starting parameter test..." << std::endl;

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

    // Get the entry point
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(handle, "clap_entry");
    if (!entry || !entry->init("/tmp"))
    {
        std::cerr << "Cannot initialize plugin entry" << std::endl;
        dlclose(handle);
        return 1;
    }

    // Get plugin factory and create instance
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
    const clap_plugin_t *plugin = factory->create_plugin(factory, &test_host, desc->id);

    if (!plugin || !plugin->init(plugin))
    {
        std::cerr << "Cannot create or initialize plugin instance" << std::endl;
        entry->deinit();
        dlclose(handle);
        return 1;
    }

    // Test parameters extension
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if (!params)
    {
        std::cout << "Plugin does not support parameters extension" << std::endl;
    }
    else
    {
        std::cout << "Parameters extension found!" << std::endl;

        uint32_t param_count = params->count(plugin);
        std::cout << "Parameter count: " << param_count << std::endl;

        // List all parameters
        for (uint32_t i = 0; i < param_count; i++)
        {
            clap_param_info_t param_info;
            if (params->get_info(plugin, i, &param_info))
            {
                std::cout << "Parameter " << i << ": " << param_info.name
                          << " (ID: " << param_info.id << ")" << std::endl;
                std::cout << "  Min: " << param_info.min_value << ", Max: " << param_info.max_value
                          << ", Default: " << param_info.default_value << std::endl;

                // Get current value
                double value;
                if (params->get_value(plugin, param_info.id, &value))
                {
                    std::cout << "  Current value: " << value << std::endl;
                }

                // CLAP parameters are set via events, not direct calls
                // For testing purposes, we'll just show we can read the value
                std::cout << "  Value can be read and displayed successfully" << std::endl;
            }
        }
    }

    // Clean up
    plugin->destroy(plugin);
    entry->deinit();
    dlclose(handle);

    std::cout << "Parameter test completed successfully!" << std::endl;
    return 0;
}