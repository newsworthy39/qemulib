#include <iostream>
#include <qemu-hypervisor.hpp>
#include <qemu-link.hpp>
#include <yaml-cpp/yaml.h>
#include <qemu.hpp>

template <typename... Args>
std::string m3_string_format(const std::string &format, Args... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0)
    {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

/**
 * @brief globals (type globals(YAML::Node &configuration, std::string section, std::string key, type defaults)
 * globals, reads configurations from a top-level element in a YAML-configuration, to supersede
 * defaults with new possibilities.
 *
 * @tparam type
 * @param configuration a YAML-node, loaded via yaml.parse or similar.
 * @param section the section in the yamlæ-node, containing a array with values.
 * @param key
 * @param defaults
 * @return type
 */
template <class type>
type globals(YAML::Node &configuration, std::string section, std::string key, type defaults)
{
    if (!configuration[section])
    {
        return defaults;
    }

    YAML::Node global = configuration[section];
    return global[key].as<type>();
}

/**
 * @brief loadMapFromRegistry (Yaml::Node &Config, const std::string section-key)
 * This function, decodes a yaml-section, from a yaml-node.
 * @tparam t1
 * @tparam t2
 * @param config Yaml::Node
 * @param key std::string
 * @return std::vector<std::tuple<t1, t2>>
 */
template <class t1, class t2>
std::vector<std::tuple<t1, t2>> loadMapFromRegistry(YAML::Node &config, const std::string key)
{
    std::vector<std::tuple<t1, t2>> vec;
    if (!config[key])
    {
        return vec;
    }

    YAML::Node node = config[key];

    if (node.Type() == YAML::NodeType::Sequence)
    {
        for (std::size_t i = 0; i < node.size(); i++)
        {
            if (node.Type() == YAML::NodeType::Sequence)
            {
                YAML::Node seq = node[i];
                for (std::size_t j = 0; j < seq.size(); j++)
                {
                    for (YAML::const_iterator it = seq.begin(); it != seq.end(); ++it)
                    {
                        vec.push_back(std::make_tuple<t1, t2>(it->first.as<t1>(), it->second.as<t2>()));
                    }
                }
            }
        }
    }

    return vec;
}

/**
 * @brief loadstores from yaml config (registry.yaml)
 *
 * @param config
 * @return std::vector<std::tuple<std::string, std::string>>
 */
std::vector<std::tuple<std::string, std::string>> loadstores(YAML::Node &config)
{
    return loadMapFromRegistry<std::string, std::string>(config, "datastores");
}

/**
 * @brief loadimages from yaml config (registry.yaml)
 *
 * @param config
 * @return std::vector<std::tuple<std::string, std::string>>
 */
std::vector<std::tuple<std::string, std::string>> loadimages(YAML::Node &config)
{
    return loadMapFromRegistry<std::string, std::string>(config, "images");
}

/**
 * @brief Operator overloading of the model <<, allowing it to be loaded
 * using standard operator functions, for readability.
 *
 * @param node
 * @param model
 */
void operator>>(const YAML::Node &node, struct Model &model)
{
    model.name = node["name"].as<std::string>();
    model.memory = node["memory"].as<int>();
    model.cpus = node["cpus"].as<int>();
    model.arch = node["arch"].as<std::string>();
    model.flags = node["flags"].as<std::string>();
}

/**
 * @brief loadModels(YAML::Node &config).
 * Loads CPU Models from the YAML-configuration.
 *
 * @param config
 * @return std::vector<struct Model>
 */
std::vector<struct Model> loadModels(YAML::Node &config)
{
    const std::string key = "models";
    std::vector<struct Model> vec;
    if (!config[key])
    {
        return vec;
    }

    YAML::Node node = config[key];

    std::for_each(node.begin(), node.end(), [&vec](const struct YAML::Node &node)
                  {
        struct Model model;
        node >> model;
        vec.push_back(model); });

    return vec;
}

std::ostream &operator<<(std::ostream &os, const struct Network &net)
{
    switch (net.topology)
    {
    case NetworkTopology::Bridge:
    {
        os << "Bridge network association: " << net.name << " network " << net.cidr;
    }
    break;
    case NetworkTopology::Macvtap:
    {
        os << "Macvtap network association: " << net.name << ", master interface " << net.interface << ", mode: " << net.macvtapmode;
    }
    break;
    }

    os << ", namespace " << net.net_namespace;

    return os;
}

/**
 * @brief Operator overloading of the model >>, allowing it to be loaded
 * using standard operator functions, for readability.
 *
 * @param node
 * @param net
 */
void operator>>(const YAML::Node &node, struct Network &net)
{

    std::string tp = node["topology"].as<std::string>();
    net.name = node["name"].as<std::string>();
    net.net_namespace = "default";

    if (node["namespace"])
    {
        net.net_namespace = node["namespace"].as<std::string>();
    }

    if (tp.compare("bridge") == 0)
    {
        net.topology = NetworkTopology::Bridge;
        if (node["cidr"])
        {
            net.cidr = node["cidr"].as<std::string>();
        }
        else
        {
            std::cerr << "When using bridge-mode, cidr is required" << std::endl;
        }
        if (node["nat"])
        {
            net.nat = node["nat"].as<bool>();
        }
    }
    if (tp.compare("macvtap") == 0)
    {
        net.topology = NetworkTopology::Macvtap;
        net.macvtapmode = NetworkMacvtapMode::Private;

        if (node["interface"])
        {
            net.interface = node["interface"].as<std::string>();
        }
        else
        {
            std::cerr << "When using macvtap-mode, interface is required" << std::endl;
        }

        if (node["mode"])
        {
            std::string mode = node["mode"].as<std::string>();
            if (mode.compare("bridge") == 0)
            {
                net.macvtapmode == NetworkMacvtapMode::Bridged;
            }
            if (mode.compare("vepa") == 0)
            {
                net.macvtapmode == NetworkMacvtapMode::VEPA;
            }
            if (mode.compare("private") == 0)
            {
                net.macvtapmode == NetworkMacvtapMode::Private;
            }
        }
    }
}

/**
 * @brief loadModels(YAML::Node &config).
 * Loads CPU Models from the YAML-configuration.
 *
 * @param config
 * @return std::vector<struct Model>
 */
std::vector<struct Network> loadNetworks(YAML::Node &config)
{
    const std::string key = "networks";
    YAML::Node node = config[key];
    std::vector<struct Network> vec;

    std::for_each(node.begin(), node.end(), [&vec](const struct YAML::Node &node)
                  {
        struct Network network  { .cidr = "10.0.96.2/24", .nat = true };
        node >> network;
        vec.push_back(network); });

    return vec;
}
/**
 * @brief main. Default program EP.
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char *argv[])
{

    // Lots of defaults.
    std::vector<std::tuple<std::string, std::string>> drives{
        {"default", "ubuntu2004backingfile"},
    };

    std::vector<std::tuple<std::string, std::string>> datastores{
        {"main", "/home/gandalf/vms"},
        {"tmp", "/tmp"},
        {"iso", "/home/gandalf/Applications"},
    };

    std::vector<struct Model> models = {
        {.name = "t1-small", .memory = 1024, .cpus = 1, .flags = "host", .arch = "amd64"},
        {.name = "t1-medium", .memory = 2048, .cpus = 2, .flags = "host", .arch = "amd64"},
        {.name = "t1-large", .memory = 4096, .cpus = 4, .flags = "host", .arch = "amd64"},
    };

    std::vector<struct Network> networks = {
        {.topology = NetworkTopology::Bridge, .name = "default", .net_namespace = "default", .cidr = "10.0.96.1/24"},
    };

    QemuContext ctx;
    int port = 4444, snapshot = 0, mandatory = 0, registerdesktop = 0;
    std::string instanceid;
    QEMU_DISPLAY display = QEMU_DISPLAY::GTK;
    YAML::Node config = YAML::LoadFile("/home/gandalf/workspace/qemu/registry.yml");
    std::string lang = globals<std::string>(config, "globals", "language", "en");
    std::string model = globals<std::string>(config, "globals", "default_instance", "t1-small");
    std::string machine = globals<std::string>(config, "globals", "default_machine", "ubuntu-q35");
    std::string disksize = globals<std::string>(config, "globals", "default_disk_size", "32g");
    std::string default_datastore = globals<std::string>(config, "globals", "default_datastore", "default");
    std::string default_isostore = globals<std::string>(config, "globals", "default_isostore", "iso");
    std::string default_network = globals<std::string>(config, "globals", "default_network", "default");
    std::string usage = m3_string_format("usage(): %s (-help) (-headless) (-snapshot) -incoming {default=4444} "
                                         "-model {default=%s} (-network default=%s+1} -machine {default=%s} -register "
                                         "(-iso cdrom) -drive hd+1 instance://instance-id { eg. instance://i-1234 }",
                                         argv[0], model.c_str(), default_network.c_str(), machine.c_str());

    struct NetworkDevice
    {
        std::string device;
        std::string netspace;
    };
    std::vector<struct NetworkDevice> devices;

    std::vector<std::tuple<std::string, std::string>> stores = loadstores(config);
    if (stores.size() > 0)
    {
        datastores = stores;
    }

    std::vector<std::tuple<std::string, std::string>> images = loadimages(config);
    if (images.size() > 0)
    {
        drives = images;
    }

    std::vector<struct Model> md = loadModels(config);
    if (md.size() > 0)
    {
        models = md;
    }

    std::vector<struct Network> nets = loadNetworks(config);
    if (nets.size() > 0)
    {
        networks = nets;
    }

    // Remember argv[0] is the path to the program, we want from argv[1] onwards
    for (int i = 1; i < argc; ++i)
    {

        if (std::string(argv[i]).find("-register") != std::string::npos)
        {
            registerdesktop = 1;
        }

        if (std::string(argv[i]).find("-help") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(EXIT_FAILURE);
        }

        if (std::string(argv[i]).find("-headless") != std::string::npos)
        {
            display = QEMU_DISPLAY::VNC;
        }

        if (std::string(argv[i]).find("-model") != std::string::npos && (i + 1 < argc))
        {
            model = argv[i + 1];

            if (std::string("?").compare(model) == 0)
            {
                std::cout << "Available models: " << std::endl;
                std::for_each(models.begin(), models.end(), [](const struct Model &mod)
                              { std::cout << mod << std::endl; });

                exit(EXIT_FAILURE);
            }
        }

        // TODO: This is probably, not the best way now.
        if (std::string(argv[i]).find("-network") != std::string::npos && (i + 1 < argc))
        {
            std::string networkname = argv[i + 1];

            if (std::string("?").compare(networkname) == 0)
            {
                std::cout << "Available networks: " << std::endl;
                std::for_each(networks.begin(), networks.end(), [](const struct Network &net)
                              { std::cout << net << std::endl; });

                exit(EXIT_FAILURE);
            }

            auto it = std::find_if(networks.begin(), networks.end(), [&networkname](const struct Network &net)
                                   { return net.name.compare(networkname) == 0; });

            if (it != networks.end())
            {
                QEMU_set_namespace((*it).net_namespace);

                if ((*it).topology == NetworkTopology::Bridge)
                {
                    int bridgeresult = QEMU_allocate_bridge(m3_string_format("br-%s", (*it).name.c_str()));
                    if (bridgeresult == 1)
                    {
                        std::cerr << "Bridge allocation error." << std::endl;
                        exit(EXIT_FAILURE);
                    }
                    QEMU_link_up(m3_string_format("br-%s", (*it).name.c_str()));
                    QEMU_set_interface_cidr(m3_string_format("br-%s", (*it).name.c_str()), (*it).cidr);
                    std::string tapdevice = QEMU_allocate_tun(ctx);
                    QEMU_link_up(tapdevice);
                    QEMU_enslave_interface(m3_string_format("br-%s", (*it).name.c_str()), tapdevice);

                    if ((*it).nat)
                    {
                        QEMU_set_router((*it).nat);
                        QEMU_iptables_set_masquerade((*it).cidr);
                    }

                    struct NetworkDevice netdevice = {.device = tapdevice, .netspace = (*it).net_namespace};
                    devices.push_back(netdevice);
                }
                if ((*it).topology == NetworkTopology::Macvtap)
                {
                    std::string tapdevice = QEMU_allocate_macvtap(ctx, (*it).interface);
                    QEMU_link_up(tapdevice);

                    struct NetworkDevice netdevice = {.device = tapdevice, .netspace = (*it).net_namespace};
                    devices.push_back(netdevice);
                }

                QEMU_set_default_namespace();
            }
        }

        if (std::string(argv[i]).find("-machine") != std::string::npos && (i + 1 < argc))
        {
            machine = argv[i + 1];
        }

        if (std::string(argv[i]).find("-iso") != std::string::npos && (i + 1 < argc))
        {

            // This allows us, to use different datastores, following this idea
            // -drive main:test-something-2.
            std::string datastore = default_isostore;
            std::string drivename = std::string(argv[i + 1]);
            const std::string delimiter = ":";

            if (drivename.find(delimiter) != std::string::npos)
            {
                datastore = drivename.substr(0, drivename.find(delimiter));  // remove the drivename-part.
                drivename = drivename.substr(drivename.find(delimiter) + 1); // remove the datastore-part.
            }

            auto it = std::find_if(datastores.begin(), datastores.end(), [&datastore](const std::tuple<std::string, std::string> &ct)
                                   { return datastore.compare(std::get<0>(ct)) == 0; });
            if (it != datastores.end())
            {
                std::string drive = m3_string_format("%s/%s.iso", std::get<1>(*it).c_str(), drivename.c_str());
                QEMU_iso(ctx, drive);
            }
            else
            {
                std::cerr << "The iso-store " << datastore << ", was not found." << std::endl;
                exit(-1);
            }
        }

        // This allows us, to use different datastores, following this idea
        // -drive main:test-something-2.
        if (std::string(argv[i]).find("-drive") != std::string::npos && (i + 1 < argc))
        {
            std::string datastore = default_datastore;
            std::string drivename = std::string(argv[i + 1]);
            const std::string delimiter = ":";

            if (drivename.find(delimiter) != std::string::npos)
            {
                datastore = drivename.substr(0, drivename.find(delimiter));  // remove the drivename-part.
                drivename = drivename.substr(drivename.find(delimiter) + 1); // remove the datastore-part.
            }

            auto dt = std::find_if(datastores.begin(), datastores.end(), [&datastore](const std::tuple<std::string, std::string> &ct)
                                   { return datastore.starts_with(std::get<0>(ct)); });
            datastore = std::get<1>(*dt);

            auto it = std::find_if(drives.begin(), drives.end(), [&drivename](const std::tuple<std::string, std::string> &ct)
                                   { return drivename.starts_with(std::get<0>(ct)); });

            std::string absdrive = m3_string_format("%s/%s.img", datastore.c_str(), drivename.c_str());

            if (it != drives.end())
            {
                std::tuple<std::string, std::string> conf = *it;
                std::string backingdrive = std::get<1>(conf);

                if (!fileExists(absdrive))
                {
                    QEMU_allocate_backed_drive(absdrive, disksize, backingdrive);
                }
            }
            else // if backing-file was found, simply blank a disksize drive.
            {
                if (!fileExists(absdrive))
                {
                    QEMU_allocate_drive(absdrive, disksize);
                }
            }

            if (-1 == QEMU_drive(ctx, absdrive))
            {
                exit(EXIT_FAILURE);
            }
        }

        if (std::string(argv[i]).find("-incoming") != std::string::npos && (i + 1 < argc))
        {
            QEMU_Accept_Incoming(ctx, std::atoi(argv[i + 1]));
        }

        if (std::string(argv[i]).find("-snapshot") != std::string::npos)
        {
            QEMU_ephimeral(ctx);
        }

        const std::string delimiter = "://";
        if (std::string(argv[i]).find(delimiter) != std::string::npos)
        {
            instanceid = std::string(argv[i]).substr(std::string(argv[i]).find(delimiter) + 3);

            auto it = std::find_if(drives.begin(), drives.end(), [&instanceid](const std::tuple<std::string, std::string> &ct)
                                   { return instanceid.starts_with(std::get<0>(ct)); });

            std::string absdrive = m3_string_format("%s/%s.img", std::get<1>(datastores.front()).c_str(), instanceid.c_str());

            if (it != drives.end())
            {
                std::tuple<std::string, std::string> conf = *it;
                std::string backingdrive = std::get<1>(conf);

                if (!fileExists(absdrive))
                {
                    QEMU_allocate_backed_drive(absdrive, disksize, backingdrive);
                }
            }
            else // if backing-file was found, simply blank a disksize drive.
            {
                if (!fileExists(absdrive))
                {
                    QEMU_allocate_drive(absdrive, disksize);
                }
            }

            mandatory = 1;

            QEMU_bootdrive(ctx, absdrive); // root-disk, is allways id=0.

            std::size_t str_hash = std::hash<std::string>{}(instanceid);
            std::string hostname = generatePrefixedUniqueString("i", str_hash, 8);
            std::string instance = generatePrefixedUniqueString("i", str_hash, 32);

            QEMU_cloud_init_default(ctx, hostname, instance);
        }
    }

    if (mandatory == 0)
    {
        std::cerr << "Error: Missing mandatory fields" << std::endl;
        std::cout << usage << std::endl;
        exit(EXIT_FAILURE);
    }

    if (registerdesktop == 1)
    {
        std::vector<std::string> arguments(argv, argv + argc);

        const char *const delim = " ";
        std::ostringstream imploded;
        std::copy(arguments.begin(), arguments.end(),
                  std::ostream_iterator<std::string>(imploded, delim));
        std::string fmt(reinterpret_cast<char *>(resources_template_desktop));
        std::ofstream desktopfile;
        desktopfile.open(m3_string_format("/home/gandalf/.local/share/applications/qemu-%s.desktop", instanceid.c_str()));
        desktopfile << m3_string_format(fmt, instanceid.c_str(), instanceid.c_str(), imploded.str().c_str()) << std::endl;
        desktopfile.close();
    }

    // Autoapply the Model.
    auto it = std::find_if(models.begin(), models.end(), [&model](const struct Model &line)
                           { return line.name.compare(model) == 0; });

    if (it != models.end())
    {
        ctx.model = *it;
    }
    else
    {
        ctx.model = *models.begin();
    }

    std::cout << "Using model: " << ctx.model.name << ", cpus: " << ctx.model.cpus << ", memory: " << ctx.model.memory << ", flags: " << ctx.model.flags << std::endl;

    pid_t daemon = fork();
    if (daemon == 0)
    {
        QEMU_instance(ctx, lang);
        QEMU_display(ctx, display);
        QEMU_machine(ctx, machine);

        QEMU_notified_started(ctx);

        pid_t child = fork();
        if (child == 0)
        {
            QEMU_launch(ctx, true); // where qemu-launch, does block, ie we can wait for it.
        }
        else
        {
            int status = 0;
            pid_t w = waitpid(child, &status, WUNTRACED | WCONTINUED);
            if (WIFEXITED(status))
            {
                std::for_each(
                    devices.begin(), devices.end(), [&ctx](const struct NetworkDevice &net)
                    {
                        QEMU_set_namespace(net.netspace);
                        QEMU_delete_link(ctx, net.device);
                        QEMU_set_default_namespace(); });

                QEMU_notified_exited(ctx);
            }

            return EXIT_SUCCESS;
        }
    }
}