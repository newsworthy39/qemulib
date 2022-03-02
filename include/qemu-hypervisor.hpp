#ifndef __QEMU_HYPERVISOR_HPP__
#define __QEMU_HYPERVISOR_HPP__

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include <fcntl.h>
#include <map>
#include <cstring>
#include <random>
#include <filesystem>
#include <fstream>
#include <algorithm>

struct Model
{
    std::string name;
    unsigned int memory;
    unsigned int cpus;
    std::string flags;
    std::string arch;
};
std::ostream& operator<<(std::ostream &os, const struct Model &model);
struct QemuContext
{
    struct Model model = { .name = "t1-small", .memory = 1024, .cpus = 1, .flags = "host" ,.arch = "amd64"};
    std::vector<std::string> devices;
    std::vector<std::string> drives;
    std::string rootdrive;
};

enum QEMU_DISPLAY
{
    GTK,
    VNC,
    NONE
};

#define QEMU_DEFAULT_IMG "/usr/bin/qemu-img"
#define QEMU_DEFAULT_SYSTEM "/usr/bin/qemu-system-x86_64"

/** Stack functions */
void PushDeviceArgument(QemuContext &args, std::string value);
void PushDriveArgument(QemuContext &ctx, std::string value);
void PushArguments(QemuContext &args, std::string key, std::string value);

/*
 * QEMU_init (std::vector<std::string>, int memory, int numcpus, const std::string language)
 */
void QEMU_instance(QemuContext &ctx, const std::string language = "en");

/*
 * QEMU_drive (QemuContext )
 * Installs a drive as the non-bootable drive (and data-drive).
 */
int QEMU_drive(QemuContext &args, const std::string &drivepath);

/*
 * QEMU_bootdrive (QemuContext )
 * Installs a drive as the bootable drive (and os-drive).
 */
int QEMU_bootdrive(QemuContext &args, const std::string &drivepath);

/*
 * QEMU_machine (QemuContext &args, const std::string model)
 */
void QEMU_machine(QemuContext &args, const std::string model);

/*
 * QEMU_iso (QemuContext &args, const std::string &model, const std::string &dabase)
 */
void QEMU_iso(QemuContext &ctx, const std::string path);

/**
 * QEMU_launch(QemuContext& args, std::string tapname, bool daemonize)
 * Launches qemu process, blocking if block is set to true. Defaults is to non-block and return immediately.
 */
void QEMU_launch(QemuContext &args, bool block = true, const std::string hypervisor = QEMU_DEFAULT_SYSTEM);

/**
 * QEMU_Display(std::vector<std::string> &args, const QEMU_DISPLAY& display);
 */
void QEMU_display(QemuContext &args, const QEMU_DISPLAY &display);

/**
 * QEMU_allocate_backed_drive(std::string id, ssize_t sz,  std::string backingid,)
 * Will create a new image, but refuse to overwrite old ones, based on a backing id
 */
void QEMU_allocate_backed_drive(std::string id, std::string sz, std::string backingfile);

/**
 * QEMU_allocate_drive(std::string id, ssize_t sz)
 * Will create a new image, but refuse to overwrite old ones.
 */
void QEMU_allocate_drive(std::string id, std::string sz);

/**
 * QEMU_rebase_backed_drive(std::string id, std::string backingpath)
 * Will rebase image onto a backing-image, but only while offline.
 */
void QEMU_rebase_backed_drive(std::string id, std::string backingfilepath);

/**
 * QEMU_notified_exited
 */
void QEMU_notified_exited(QemuContext &ctx);

/*
 * QEMU_notified_started()
 */
void QEMU_notified_started(QemuContext &ctx);

/*
 * QEMU_notified_started()
 */
void QEMU_Notify_Register(QemuContext &ctx);

/*
 * QEMU_notified_started()
 */
void QEMU_Notify_Unregister(QemuContext &ctx);

/**
 * QEMU_accept_incoming
 */
void QEMU_Accept_Incoming(QemuContext &ctx, int port);

/*
 * QEMU_ephimeral()
 */
void QEMU_ephimeral(QemuContext &ctx);

/**
 * QEMU_Guest_Id
 * Get (or create) a guest-id (uuidv4).
 */
std::string QEMU_reservation_id(QemuContext &ctx);

/**
 * QEMU_Notify_started
 * Sets the cloud-init arguments source
 */
void QEMU_cloud_init_network(QemuContext &ctx, std::string cloud_settings_src);

/**
 * QEMU_Notify_started
 * Sets the cloud-init arguments source
 */
void QEMU_cloud_init_file(QemuContext &ctx, std::string hostname, std::string instance_id);

/**
 * QEMU_Cluod_init_arguments
 * Removes any cloud-init arguments
 * serial=ds=None
 */
void QEMU_cloud_init_remove(QemuContext &ctx);

/**
 * QEMU_cloud_init_default
 * serial=ds=None
 */
void QEMU_cloud_init_default(QemuContext &ctx, std::string instanceid);

/**
 * @brief QEMU_oemstrings
 * adds and sets oem-strings inside the bios for a VM.
 * 
 * @param ctx QemuContext
 * @param oemstrings  std::vector<std::string> formatted as the value={}-part.
 */
void QEMU_oemstring(QemuContext &ctx, std::vector<std::string> oemstrings);

bool fileExists(const std::string &filename);

/**
 * generateRandomPrefixedString(std::string prefix, int length)
 */
std::string generateRandomPrefixedString(std::string prefix, int length);

/**
 * generatePrefixedUniqueString(std::string prefix, int length)
 */
std::string generatePrefixedUniqueString(std::string prefix, std::size_t hash, unsigned int length);

/**
 * QEMU_get_pid
 * get pid of running hypervisor.
 */
pid_t QEMU_get_pid(std::string &reservationid);

/**
 * @brief QEMU_get_reservations()
 * @returns std::vector<std::string>
 */
std::vector<std::string> QEMU_get_reservations();

#endif
