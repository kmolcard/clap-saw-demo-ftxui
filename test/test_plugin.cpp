#include <iostream>
#include <dlfcn.h>
#include <clap/clap.h>

int main(int argc, char *argv[])
{
    std::cout << "Starting plugin test..." << std::endl;

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

    // Get the entry point (it's a structure, not a function)
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(handle, "clap_entry");
    if (!entry)
    {
        std::cerr << "Cannot find clap_entry symbol: " << dlerror() << std::endl;
        dlclose(handle);
        return 1;
    }
    std::cout << "Found clap_entry symbol" << std::endl;
    if (!entry)
    {
        std::cerr << "clap_entry is null" << std::endl;
        dlclose(handle);
        return 1;
    }
    std::cout << "clap_entry found successfully" << std::endl;

    std::cout << "Successfully loaded CLAP plugin!" << std::endl;
    std::cout << "Entry version: " << entry->clap_version.major << "." << entry->clap_version.minor
              << "." << entry->clap_version.revision << std::endl;

    // Initialize the plugin
    std::cout << "Initializing plugin..." << std::endl;
    if (!entry->init("/tmp"))
    {
        std::cerr << "Failed to initialize plugin" << std::endl;
        dlclose(handle);
        return 1;
    }
    std::cout << "Plugin initialized successfully" << std::endl;

    // Get plugin factory
    std::cout << "Getting plugin factory..." << std::endl;
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory)
    {
        std::cerr << "Cannot get plugin factory" << std::endl;
        entry->deinit();
        dlclose(handle);
        return 1;
    }

    std::cout << "Plugin factory found, plugin count: " << factory->get_plugin_count(factory)
              << std::endl;

    if (factory->get_plugin_count(factory) > 0)
    {
        const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, 0);
        if (desc)
        {
            std::cout << "Plugin: " << desc->name << std::endl;
            std::cout << "  ID: " << desc->id << std::endl;
            std::cout << "  Features: ";
            for (int i = 0; desc->features[i]; i++)
            {
                std::cout << desc->features[i] << " ";
            }
            std::cout << std::endl;
        }
    }

    // Clean up
    std::cout << "Cleaning up..." << std::endl;
    entry->deinit();
    dlclose(handle);

    std::cout << "Plugin test completed successfully!" << std::endl;
    return 0;
}
