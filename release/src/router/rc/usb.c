/*

	USB Support

*/
#include "rc.h"

#ifdef RTCONFIG_USB
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <sys/mount.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/file.h>
#include <sys/swap.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <rc_event.h>
#include <shared.h>

#include <usb_info.h>
#include <disk_io_tools.h>
#include <disk_share.h>
#include <disk_initial.h>

#include <bin_sem_asus.h>

char *usb_dev_file = "/proc/bus/usb/devices";

#define USB_CLS_PER_INTERFACE	0	/* for DeviceClass */
#define USB_CLS_AUDIO		1
#define USB_CLS_COMM		2
#define USB_CLS_HID		3
#define USB_CLS_PHYSICAL	5
#define USB_CLS_STILL_IMAGE	6
#define USB_CLS_PRINTER		7
#define USB_CLS_MASS_STORAGE	8
#define USB_CLS_HUB		9
#define USB_CLS_CDC_DATA	0x0a
#define USB_CLS_CSCID		0x0b	/* chip+ smart card */
#define USB_CLS_CONTENT_SEC	0x0d	/* content security */
#define USB_CLS_VIDEO		0x0e
#define USB_CLS_WIRELESS_CONTROLLER	0xe0
#define USB_CLS_MISC		0xef
#define USB_CLS_APP_SPEC	0xfe
#define USB_CLS_VENDOR_SPEC	0xff
#define USB_CLS_3GDEV		0x35

#define OP_MOUNT		1
#define OP_UMOUNT		2
#define OP_SETNVRAM		3

char *find_sddev_by_mountpoint(char *mountpoint);

/* Adjust bdflush parameters.
 * Do this here, because Tomato doesn't have the sysctl command.
 * With these values, a disk block should be written to disk within 2 seconds.
 */
#ifdef LINUX26
void tune_bdflush(void)
{
	f_write_string("/proc/sys/vm/dirty_expire_centisecs", "200", 0, 0);
	f_write_string("/proc/sys/vm/dirty_writeback_centisecs", "200", 0, 0);
}
#else
#include <sys/kdaemon.h>
#define SET_PARM(n) (n * 2 | 1)
void tune_bdflush(void)
{
	bdflush(SET_PARM(5), 100);
	bdflush(SET_PARM(6), 100);
	bdflush(SET_PARM(8), 0);
}
#endif // LINUX26

#define USBCORE_MOD	"usbcore"
#define USB20_MOD	"ehci-hcd"
#define USBSTORAGE_MOD	"usb-storage"
#define SCSI_MOD	"scsi_mod"
#define SD_MOD		"sd_mod"
#define SG_MOD		"sg"
#ifdef LINUX26
#define USBOHCI_MOD	"ohci-hcd"
#define USBUHCI_MOD	"uhci-hcd"
#define USBPRINTER_MOD	"usblp"
#define SCSI_WAIT_MOD	"scsi_wait_scan"
#define USBFS		"usbfs"
#else
#define USBOHCI_MOD	"usb-ohci"
#define USBUHCI_MOD	"usb-uhci"
#define USBPRINTER_MOD	"printer"
#define USBFS		"usbdevfs"
#endif

#ifdef RTCONFIG_USB_PRINTER
void
start_lpd()
{
	if(getpid()!=1) {
		notify_rc("start_lpd");
		return;
	}

	if (!pids("lpd"))
	{
		unlink("/var/run/lpdparent.pid");
		//return xstart("lpd");
		system("lpd &");
	}
}

void
stop_lpd()
{
	if(getpid()!=1) {
		notify_rc("stop_lpd");
		return;
	}

	if (pids("lpd"))
	{
		killall_tk("lpd");
		unlink("/var/run/lpdparent.pid");
	}
}

void
start_u2ec()
{
	if(getpid()!=1) {
		notify_rc("start_u2ec");
		return;
	}

	if (!pids("u2ec"))
	{
		unlink("/var/run/u2ec.pid");
		system("u2ec &");

		nvram_set("apps_u2ec_ex", "1");
	}
}

void
stop_u2ec()
{
	if(getpid()!=1) {
		notify_rc("stop_u2ec");
		return;
	}

	if (pids("u2ec")){
		//system("killall -SIGKILL u2ec");
		killall_tk("u2ec");
		unlink("/var/run/u2ec.pid");
	}
}
#endif

FILE* fopen_or_warn(const char *path, const char *mode)
{
	FILE *fp = fopen(path, mode);

	if (!fp)
	{
		dbg("hotplug USB: No such file or directory: %s\n", path);
		errno = 0;
		return NULL;
	}

	return fp;
}

#ifdef DLM

int
test_user(char *target, char *pattern)
{
	char s[384];
	char p[32];
	char *start;
	char *pp;
	strcpy(s, target);
	strcpy(p, pattern);
	start = s;
	while (pp=strchr(start, ';'))
	{
		*pp='\0';
		if (! strcmp(start, p))
			return 1;
		start=pp+1;
	}
	return 0;
}

#define MOUNT_VAL_FAIL 	0
#define MOUNT_VAL_RONLY	1
#define MOUNT_VAL_RW 	2

int
fill_smbpasswd_input_file(const char *passwd)
{
	FILE *fp;
	
	unlink("/tmp/smbpasswd");
	fp = fopen("/tmp/smbpasswd", "w");
	
	if (fp && passwd)
	{
		fprintf(fp,"%s\n", passwd);
		fprintf(fp,"%s\n", passwd);
		fclose(fp);
		
		return 1;
	}
	else
		return 0;
}
#endif



void start_usb(void)
{
	char param[32];
	int i;

	_dprintf("%s\n", __FUNCTION__);
#ifdef REMOVE
	tune_bdflush();
#endif
	if (nvram_get_int("usb_enable")) {
		modprobe(USBCORE_MOD);

		/* mount usb device filesystem */
        	mount(USBFS, "/proc/bus/usb", USBFS, MS_MGC_VAL, NULL);

#ifdef LINUX26
		if ((i = nvram_get_int("led_usb_gpio")) != 255) {
			modprobe("ledtrig-usbdev");
			modprobe("leds-usb");
			sprintf(param, "%d", i);
			f_write_string("/sys/class/leds/usb-led/gpio_pin", param, 0, 0);
			f_write_string("/sys/class/leds/usb-led/device_name", "1-1", 0, 0);
		}
#endif
		if (nvram_get_int("usb_storage")) {
			/* insert scsi and storage modules before usb drivers */
			modprobe(SCSI_MOD);
#ifdef LINUX26
			modprobe(SCSI_WAIT_MOD);
#endif
			modprobe(SD_MOD);
			modprobe(SG_MOD);
			modprobe(USBSTORAGE_MOD);

			if (nvram_get_int("usb_fs_ext3")) {
#ifdef LINUX26
				modprobe("mbcache");	// used by ext2/ext3
#endif
				/* insert ext3 first so that lazy mount tries ext3 before ext2 */
				modprobe("jbd");
				modprobe("ext3");
				modprobe("ext2");
			}

			if (nvram_get_int("usb_fs_fat")) {
				modprobe("fat");
				modprobe("vfat");
			}
#ifdef RTCONFIG_NTFS
#ifndef RTCONFIG_NTFS3G
			if (nvram_get_int("usb_fs_ntfs"))
				modprobe("/usr/lib/ufsd.ko");
#endif
#endif
		}

		/* if enabled, force USB2 before USB1.1 */
		if (nvram_get_int("usb_usb2") == 1) {
			i = nvram_get_int("usb_irq_thresh");
			if ((i < 0) || (i > 6))
				i = 0;
			sprintf(param, "log2_irq_thresh=%d", i);
			modprobe(USB20_MOD, param);
		}

		if (nvram_get_int("usb_uhci") == 1) {
			modprobe(USBUHCI_MOD);
		}

		if (nvram_get_int("usb_ohci") == 1) {
			modprobe(USBOHCI_MOD);
		}
#ifdef RTCONFIG_USB_PRINTER
		if (nvram_get_int("usb_printer")) {
			symlink("/dev/usb", "/dev/printers");
			modprobe(USBPRINTER_MOD);
//			start_u2ec();
//			start_lpd();
		}
#endif
#ifdef RTCONFIG_USB_MODEM
		modprobe("usbnet");
		modprobe("asix");
		modprobe("cdc_ether");
		modprobe("net1080");
		modprobe("rndis_host");
		modprobe("zaurus");
#endif
	}
}

void stop_usb(void)
{
	int disabled = !nvram_get_int("usb_enable");

#ifdef RTCONFIG_USB_MODEM
		modprobe("asix");
		modprobe("cdc_ether");
		modprobe("net1080");
		modprobe("rndis_host");
		modprobe("zaurus");
		modprobe("usbnet");
#endif

#ifdef RTCONFIG_USB_PRINTER
	stop_lpd();
	stop_u2ec();
	modprobe_r(USBPRINTER_MOD);
#endif

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	stop_app();
#endif
	stop_nas_services();

	// only stop storage services if disabled
	if (disabled || !nvram_get_int("usb_storage")) {
		// Unmount all partitions
		remove_storage_main(0);

		// Stop storage services
		modprobe_r("ext2");
		modprobe_r("ext3");
		modprobe_r("jbd");
#ifdef LINUX26
		modprobe_r("mbcache");
#endif
		modprobe_r("vfat");
		modprobe_r("fat");
#ifdef RTCONFIG_NTFS
#ifndef RTCONFIG_NTFS3G
		modprobe_r("ufsd");
#endif
#endif
		modprobe_r("fuse");
		sleep(1);
#ifdef RTCONFIG_SAMBASRV
		modprobe_r("nls_cp437");
		modprobe_r("nls_cp850");
		modprobe_r("nls_cp852");
		modprobe_r("nls_cp866");
#ifdef LINUX26
		modprobe_r("nls_cp932");
		modprobe_r("nls_cp936");
		modprobe_r("nls_cp949");
		modprobe_r("nls_cp950");
#endif
#endif
		modprobe_r(USBSTORAGE_MOD);
		modprobe_r(SD_MOD);
		modprobe_r(SG_MOD);
#ifdef LINUX26
		modprobe_r(SCSI_WAIT_MOD);
#endif
		modprobe_r(SCSI_MOD);
	}

	if (disabled || nvram_get_int("usb_ohci") != 1) modprobe_r(USBOHCI_MOD);
	if (disabled || nvram_get_int("usb_uhci") != 1) modprobe_r(USBUHCI_MOD);
	if (disabled || nvram_get_int("usb_usb2") != 1) modprobe_r(USB20_MOD);

#ifdef LINUX26
	modprobe_r("leds-usb");
	modprobe_r("ledtrig-usbdev");
#endif

	// only unload core modules if usb is disabled
	if (disabled) {
		umount("/proc/bus/usb"); // unmount usb device filesystem
		modprobe_r(USBOHCI_MOD);
		modprobe_r(USBUHCI_MOD);
		modprobe_r(USB20_MOD);
		modprobe_r(USBCORE_MOD);
	}
}


#ifdef RTCONFIG_USB_PRINTER
void start_usblpsrv()
{
	nvram_set("u2ec_device", "");
	nvram_set("u2ec_busyip", "");
	nvram_set("MFP_busy", "0");

	bin_sem_init();
	start_u2ec();
	start_lpd();
}
#endif

#define MOUNT_VAL_FAIL 	0
#define MOUNT_VAL_RONLY	1
#define MOUNT_VAL_RW 	2
#define MOUNT_VAL_EXIST	3

int mount_r(char *mnt_dev, char *mnt_dir, char *type)
{
	struct mntent *mnt;
	int ret;
	char options[140];
	char flagfn[128];
	int dir_made;

	if ((mnt = findmntents(mnt_dev, 0, NULL, 0))) {
		syslog(LOG_INFO, "USB partition at %s already mounted on %s",
			mnt_dev, mnt->mnt_dir);
		return MOUNT_VAL_EXIST;
	}

	options[0] = 0;

	if (type) {
		unsigned long flags = MS_NOATIME | MS_NODEV;

		if (strcmp(type, "swap") == 0 || strcmp(type, "mbr") == 0) {
			/* not a mountable partition */
			flags = 0;
		}
		else if (strcmp(type, "ext2") == 0 || strcmp(type, "ext3") == 0) {
			sprintf(options, "umask=0000");
			sprintf(options + strlen(options), ",user_xattr");

			if (nvram_invmatch("usb_ext_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_ext_opt"));
		}
		else if (strcmp(type, "vfat") == 0) {
			sprintf(options, "umask=0000,allow_utime=0000");

			if (nvram_invmatch("smbd_cset", ""))
				sprintf(options + strlen(options), ",iocharset=%s%s", 
					isdigit(nvram_get("smbd_cset")[0]) ? "cp" : "",
						nvram_get("smbd_cset"));
			if (nvram_invmatch("smbd_cpage", "")) {
				char *cp = nvram_safe_get("smbd_cpage");
				sprintf(options + strlen(options), ",codepage=%s" + (options[0] ? 0 : 1), cp);
				sprintf(flagfn, "nls_cp%s", cp);
				TRACE_PT("USB %s(%s) is setting the code page to %s!\n", mnt_dev, type, flagfn);

				cp = nvram_get("smbd_nlsmod");
				if ((cp) && (*cp != 0) && (strcmp(cp, flagfn) != 0))
					modprobe_r(cp);

				modprobe(flagfn);
				nvram_set("smbd_nlsmod", flagfn);
			}
			sprintf(options + strlen(options), ",shortname=winnt" + (options[0] ? 0 : 1));
#ifdef REMOVE
#ifdef LINUX26
			sprintf(options + strlen(options), ",flush" + (options[0] ? 0 : 1));
#endif
#endif
			if (nvram_invmatch("usb_fat_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_fat_opt"));

		}
		else if (strncmp(type, "ntfs", 4) == 0) {
			sprintf(options, "umask=0000");

			if (nvram_invmatch("smbd_cset", ""))
				sprintf(options + strlen(options), "%siocharset=%s%s", options[0] ? "," : "",
					isdigit(nvram_get("smbd_cset")[0]) ? "cp" : "",
						nvram_get("smbd_cset"));

			if (nvram_invmatch("usb_ntfs_opt", ""))
				sprintf(options + strlen(options), "%s%s", options[0] ? "," : "", nvram_safe_get("usb_ntfs_opt"));
		}

		if (flags) {
			if ((dir_made = mkdir_if_none(mnt_dir))) {
				/* Create the flag file for remove the directory on dismount. */
				sprintf(flagfn, "%s/.autocreated-dir", mnt_dir);
				f_write(flagfn, NULL, 0, 0, 0);
			}

			ret = mount(mnt_dev, mnt_dir, type, flags, options[0] ? options : "");
			if(ret != 0){
				syslog(LOG_INFO, "USB %s(%s) failed to mount at the first try!"
						, mnt_dev, type);
				TRACE_PT("USB %s(%s) failed to mount at the first try!\n", mnt_dev, type);
			}

#ifdef RTCONFIG_NTFS
			/* try ntfs-3g in case it's installed */
			if (ret != 0 && strncmp(type, "ntfs", 4) == 0) {
				sprintf(options + strlen(options), ",noatime,nodev" + (options[0] ? 0 : 1));

				if (nvram_get_int("usb_fs_ntfs")) {
					if(!nvram_get_int("usb_fs_ntfs3g"))
						ret = eval("mount", "-t", "ufsd", "-o", options, "-o", "force", mnt_dev, mnt_dir);
					else 
						ret = eval("ntfs-3g", "-o", options, mnt_dev, mnt_dir);
				}
			}
#endif

			if (ret != 0) /* give it another try - guess fs */
				ret = eval("mount", "-o", "noatime,nodev", mnt_dev, mnt_dir);

			if (ret == 0) {
				syslog(LOG_INFO, "USB %s%s fs at %s mounted on %s",
					type, (flags & MS_RDONLY) ? " (ro)" : "", mnt_dev, mnt_dir);
				return (flags & MS_RDONLY) ? MOUNT_VAL_RONLY : MOUNT_VAL_RW;
			}

			if (dir_made) {
				unlink(flagfn);
				rmdir(mnt_dir);
			}
		}
	}
	return MOUNT_VAL_FAIL;
}


struct mntent *mount_fstab(char *dev_name, char *type, char *label, char *uuid)
{
	struct mntent *mnt = NULL;
#if 0
	if (eval("mount", "-a") == 0)
		mnt = findmntents(dev_name, 0, NULL, 0);
#else
	char spec[PATH_MAX+1];

	if (label && *label) {
		sprintf(spec, "LABEL=%s", label);
		if (eval("mount", spec) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt && uuid && *uuid) {
		sprintf(spec, "UUID=%s", uuid);
		if (eval("mount", spec) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt) {
		if (eval("mount", dev_name) == 0)
			mnt = findmntents(dev_name, 0, NULL, 0);
	}

	if (!mnt) {
		/* Still did not find what we are looking for, try absolute path */
		if (realpath(dev_name, spec)) {
			if (eval("mount", spec) == 0)
				mnt = findmntents(dev_name, 0, NULL, 0);
		}
	}
#endif

	if (mnt)
		syslog(LOG_INFO, "USB %s fs at %s mounted on %s", type, dev_name, mnt->mnt_dir);
	return (mnt);
}


/* Check if the UFD is still connected because the links created in /dev/discs
 * are not removed when the UFD is  unplugged.
 * The file to read is: /proc/scsi/usb-storage-#/#, where # is the host no.
 * We are looking for "Attached: Yes".
 */
static int usb_ufd_connected(int host_no)
{
	char proc_file[128];
#ifndef LINUX26
	char line[256];
#endif
	FILE *fp;

	sprintf(proc_file, "%s/%s-%d/%d", PROC_SCSI_ROOT, USB_STORAGE, host_no, host_no);
	fp = fopen(proc_file, "r");

	if (!fp) {
		/* try the way it's implemented in newer kernels: /proc/scsi/usb-storage/[host] */
		sprintf(proc_file, "%s/%s/%d", PROC_SCSI_ROOT, USB_STORAGE, host_no);
		fp = fopen(proc_file, "r");
	}

	if (fp) {
#ifdef LINUX26
		fclose(fp);
		return 1;
#else
		while (fgets(line, sizeof(line), fp) != NULL) {
			if (strstr(line, "Attached: Yes")) {
				fclose(fp);
				return 1;
			}
		}
		fclose(fp);
#endif
	}

	return 0;
}


#ifndef MNT_DETACH
#define MNT_DETACH	0x00000002      /* from linux/fs.h - just detach from the tree */
#endif
int umount_mountpoint(struct mntent *mnt, uint flags);
int uswap_mountpoint(struct mntent *mnt, uint flags);

/* Unmount this partition from all its mountpoints.  Note that it may
 * actually be mounted several times, either with different names or
 * with "-o bind" flag.
 * If the special flagfile is now revealed, delete it and [attempt to] delete
 * the directory.
 */
int umount_partition(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags)
{
	sync();	/* This won't matter if the device is unplugged, though. */

	if (flags & EFH_HUNKNOWN) {
		/* EFH_HUNKNOWN flag is passed if the host was unknown.
		 * Only unmount disconnected drives in this case.
		 */
		if (usb_ufd_connected(host_num))
			return 0;
	}

	/* Find all the active swaps that are on this device and stop them. */
	findmntents(dev_name, 1, uswap_mountpoint, flags);

	/* Find all the mountpoints that are for this device and unmount them. */
	findmntents(dev_name, 0, umount_mountpoint, flags);
	return 0;
}

int uswap_mountpoint(struct mntent *mnt, uint flags)
{
	swapoff(mnt->mnt_fsname);
	return 0;
}

int umount_mountpoint(struct mntent *mnt, uint flags)
{
	int ret = 1, count;
	char flagfn[128];

	sprintf(flagfn, "%s/.autocreated-dir", mnt->mnt_dir);

	/* Run user pre-unmount scripts if any. It might be too late if
	 * the drive has been disconnected, but we'll try it anyway.
 	 */
	if (nvram_get_int("usb_automount"))
		run_nvscript("script_usbumount", mnt->mnt_dir, 3);
	/* Run *.autostop scripts located in the root of the partition being unmounted if any. */
	//run_userfile(mnt->mnt_dir, ".autostop", mnt->mnt_dir, 5);
	//run_nvscript("script_autostop", mnt->mnt_dir, 5);
#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	if(nvram_match("apps_mounted_path", mnt->mnt_dir))
		stop_app();
#endif

	count = 0;
	while ((ret = umount(mnt->mnt_dir)) && (count < 2)) {
		count++;
		/* If we could not unmount the drive on the 1st try,
		 * kill all NAS applications so they are not keeping the device busy -
		 * unless it's an unmount request from the Web GUI.
		 */
		if ((count == 1) && ((flags & EFH_USER) == 0)){
_dprintf("restart_nas_services(%d): test 1.\n", getpid());
			restart_nas_services(1, 0);
		}
		sleep(1);
	}

	if (ret == 0)
		syslog(LOG_INFO, "USB partition unmounted from %s", mnt->mnt_dir);

	if (ret && ((flags & EFH_SHUTDN) != 0)) {
		/* If system is stopping (not restarting), and we couldn't unmount the
		 * partition, try to remount it as read-only. Ignore the return code -
		 * we can still try to do a lazy unmount.
		 */
		eval("mount", "-o", "remount,ro", mnt->mnt_dir);
	}

	if (ret && ((flags & EFH_USER) == 0)) {
		/* Make one more try to do a lazy unmount unless it's an unmount
		 * request from the Web GUI.
		 * MNT_DETACH will expose the underlying mountpoint directory to all
		 * except whatever has cd'ed to the mountpoint (thereby making it busy).
		 * So the unmount can't actually fail. It disappears from the ken of
		 * everyone else immediately, and from the ken of whomever is keeping it
		 * busy when they move away from it. And then it disappears for real.
		 */
		ret = umount2(mnt->mnt_dir, MNT_DETACH);
		syslog(LOG_INFO, "USB partition busy - will unmount ASAP from %s", mnt->mnt_dir);
	}

	if (ret == 0) {
		if ((unlink(flagfn) == 0)) {
			// Only delete the directory if it was auto-created
			rmdir(mnt->mnt_dir);
		}
	}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
	if(nvram_match("apps_mounted_path", mnt->mnt_dir)){
		nvram_set("apps_dev", "");
		nvram_set("apps_mounted_path", "");
		nvram_set("apps_pool_error", "");
	}
#endif

	return (ret == 0);
}


/* Mount this partition on this disc.
 * If the device is already mounted on any mountpoint, don't mount it again.
 * If this is a swap partition, try swapon -a.
 * If this is a regular partition, try mount -a.
 *
 * Before we mount any partitions:
 *	If the type is swap and /etc/fstab exists, do "swapon -a"
 *	If /etc/fstab exists, try mounting using fstab.
 *  We delay invoking mount because mount will probe all the partitions
 *	to read the labels, and we don't want it to do that early on.
 *  We don't invoke swapon until we actually find a swap partition.
 *
 * If the mount succeeds, execute the *.asusrouter scripts in the top
 * directory of the newly mounted partition.
 * Returns NZ for success, 0 if we did not mount anything.
 */
int mount_partition(char *dev_name, int host_num, char *dsc_name, char *pt_name, uint flags)
{
	char the_label[128], mountpoint[128], uuid[40];
	int ret;
	char *type, *p;
	static char *swp_argv[] = { "swapon", "-a", NULL };
	struct mntent *mnt;

	if ((type = detect_fs_type(dev_name)) == NULL)
		return 0;

	find_label_or_uuid(dev_name, the_label, uuid);

	if(!is_valid_hostname(the_label))
		memset(the_label, 0, 128);

    run_custom_script_blocking("pre-mount", dev_name);

	if (f_exists("/etc/fstab")) {
		if (strcmp(type, "swap") == 0) {
			_eval(swp_argv, NULL, 0, NULL);
			return 0;
		}
		if (mount_r(dev_name, NULL, NULL) == MOUNT_VAL_EXIST)
			return 0;
		if ((mnt = mount_fstab(dev_name, type, the_label, uuid))) {
			strcpy(mountpoint, mnt->mnt_dir);
			ret = MOUNT_VAL_RW;
			goto done;
		}
	}

	if (*the_label != 0) {
		for (p = the_label; *p; p++) {
			if (!isalnum(*p) && !strchr("+-&.@", *p))
				*p = '_';
		}
		sprintf(mountpoint, "%s/%s", MOUNT_ROOT, the_label);

		int the_same_name = 0;
		while(test_if_file(mountpoint) || test_if_dir(mountpoint)){
			++the_same_name;
			sprintf(mountpoint, "%s/%s(%d)", MOUNT_ROOT, the_label, the_same_name);
		}

		if ((ret = mount_r(dev_name, mountpoint, type)))
			goto done;
	}

	/* Can't mount to /mnt/LABEL, so try mounting to /mnt/discDN_PN */
	sprintf(mountpoint, "%s/%s", MOUNT_ROOT, pt_name);
	ret = mount_r(dev_name, mountpoint, type);

done:
	if (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW)
	{
		chmod(mountpoint, 0777);

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
		if(!strcmp(nvram_safe_get("apps_mounted_path"), "")){
			char command[128], buff1[64], buff2[64];

			memset(buff1, 0, 64);
			sprintf(buff1, "%s/asusware/.asusrouter", mountpoint);
			memset(buff2, 0, 64);
			sprintf(buff2, "%s/asusware/.asusrouter.disabled", mountpoint);

			if(test_if_file(buff1) && !test_if_file(buff2)){
				// fsck the partition.
				if(!strcmp(type, "ext2") || !strcmp(type, "ext3")
						|| !strcmp(type, "vfat") || !strcmp(type, "msdos")
						|| !strcmp(type, "ntfs") || !strcmp(type, "ufsd")
						){
					findmntents(dev_name, 0, umount_mountpoint, EFH_HP_REMOVE);
					memset(command, 0, 128);
					sprintf(command, "app_fsck.sh %s %s", type, dev_name);
					system(command);
					mount_r(dev_name, mountpoint, type);
				}

				system("rm -rf /tmp/opt");

				memset(command, 0, 128);
				sprintf(command, "ln -sf %s/asusware /tmp/opt", mountpoint);
				system(command);

				memset(buff1, 0, 64);
				sprintf(buff1, "APPS_DEV=%s", dev_name+5);
				putenv(buff1);
				memset(buff2, 0, 64);
				sprintf(buff2, "APPS_MOUNTED_PATH=%s", mountpoint);
				putenv(buff2);
				/* Run user *.asusrouter and post-mount scripts if any. */
				memset(command, 0, 128);
				sprintf(command, "%s/asusware", mountpoint);
				run_userfile(command, ".asusrouter", NULL, 3);
				unsetenv("APPS_DEV");
				unsetenv("APPS_MOUNTED_PATH");
			}
		}
#endif

		// check the permission files.
		if(ret == MOUNT_VAL_RW)
			test_of_var_files(mountpoint);

		if (nvram_get_int("usb_automount"))
			run_nvscript("script_usbmount", mountpoint, 3);

        run_custom_script_blocking("post-mount", mountpoint);
	}
	return (ret == MOUNT_VAL_RONLY || ret == MOUNT_VAL_RW);
}

#if 0 /* LINUX26 */

/* 
 * Finds SCSI Host number. Returns the host number >=0 if found, or (-1) otherwise.
 * The name and host number of scsi block device in kernel 2.6 (for attached devices) can be found as
 * 	/sys($DEVPATH)/host<host_no>/target<*>/<id>/block:[sda|sdb|...]
 * where $DEVPATH is passed to hotplug events, and looks like
 * 	/devices/pci0000:00/0000:00:04.1/usb1/1-1/1-1:1.2
 *
 * For printers this function finds a minor assigned to a printer
 *	/sys($DEVPATH)/usb:lp[0|1|2|...]
 */
int find_dev_host(const char *devpath)
{
	DIR *usb_devpath;
	struct dirent *dp;
	char buf[256];
	int host = -1;	/* Scsi Host */

	sprintf(buf, "/sys%s", devpath);
	if ((usb_devpath = opendir(buf))) {
		while ((dp = readdir(usb_devpath))) {
			errno = 0;
			if (strncmp(dp->d_name, "host", 4) == 0) {
				host = strtol(dp->d_name + 4, (char **)NULL, 10);
				if (errno)
					host = -1;
				else
					break;
			}
			else if (strncmp(dp->d_name, "usb:lp", 6) == 0) {
				host = strtol(dp->d_name + 6, (char **)NULL, 10);
				if (errno)
					host = -1;
				else
					break;
			}
			else
				continue;
		}
		closedir(usb_devpath);
	}
	return (host);
}

#endif	/* LINUX26 */


/* Mount or unmount all partitions on this controller.
 * Parameter: action_add:
 * 0  = unmount
 * >0 = mount only if automount config option is enabled.
 * <0 = mount regardless of config option.
 */
void hotplug_usb_storage_device(int host_no, int action_add, uint flags)
{
	if (!nvram_get_int("usb_enable"))
		return;

	_dprintf("%s: host %d action: %d\n", __FUNCTION__, host_no, action_add);

	if (action_add) {
		if (nvram_get_int("usb_storage") && (nvram_get_int("usb_automount") || action_add < 0)) {
			/* Do not probe the device here. It's either initiated by user,
			 * or hotplug_usb() already did.
			 */
			if (exec_for_host(host_no, 0x00, flags, mount_partition)) {
_dprintf("restart_nas_services(%d): test 2.\n", getpid());
				restart_nas_services(1, 1); // restart all NAS applications
			}
		}
	}
	else {
		if (nvram_get_int("usb_storage") || ((flags & EFH_USER) == 0)) {
			/* When unplugged, unmount the device even if
			 * usb storage is disabled in the GUI.
			 */
			exec_for_host(host_no, (flags & EFH_USER) ? 0x00 : 0x02, flags, umount_partition);
			/* Restart NAS applications (they could be killed by umount_mountpoint),
			 * or just re-read the configuration.
			 */
_dprintf("restart_nas_services(%d): test 3.\n", getpid());
			restart_nas_services(1, 1);
		}
	}
}


/* This gets called at reboot or upgrade.  The system is stopping. */
void remove_storage_main(int shutdn)
{
	if (shutdn){
_dprintf("restart_nas_services(%d): test 4.\n", getpid());
		restart_nas_services(1, 0);
	}
	/* Unmount all partitions */
	exec_for_host(-1, 0x02, shutdn ? EFH_SHUTDN : 0, umount_partition);
}


/*******
 * All the complex locking & checking code was removed when the kernel USB-storage
 * bugs were fixed.
 * The crash bug was with overlapped I/O to different USB drives, not specifically
 * with mount processing.
 *
 * And for USB devices that are slow to come up.  The kernel now waits until the
 * USB drive has settled, and it correctly reads the partition table before calling
 * the hotplug agent.
 *
 * The kernel patch was cleaning up data structures on an unplug.  It
 * needs to wait until the disk is unmounted.  We have 20 seconds to do
 * the unmounts.
 *******/


/* Plugging or removing usb device
 *
 * On an occurrance, multiple hotplug events may be fired off.
 * For example, if a hub is plugged or unplugged, an event
 * will be generated for everything downstream of it, plus one for
 * the hub itself.  These are fired off simultaneously, not serially.
 * This means that many many hotplug processes will be running at
 * the same time.
 *
 * The hotplug event generated by the kernel gives us several pieces
 * of information:
 * PRODUCT is vendorid/productid/rev#.
 * DEVICE is /proc/bus/usb/bus#/dev#
 * ACTION is add or remove
 * SCSI_HOST is the host (controller) number (this relies on the custom kernel patch)
 *
 * Note that when we get a hotplug add event, the USB susbsystem may
 * or may not have yet tried to read the partition table of the
 * device.  For a new controller that has never been seen before,
 * generally yes.  For a re-plug of a controller that has been seen
 * before, generally no.
 *
 * On a remove, the partition info has not yet been expunged.  The
 * partitions show up as /dev/discs/disc#/part#, and /proc/partitions.
 * It appears that doing a "stat" for a non-existant partition will
 * causes the kernel to re-validate the device and update the
 * partition table info.  However, it won't re-validate if the disc is
 * mounted--you'll get a "Device busy for revalidation (usage=%d)" in
 * syslog.
 *
 * The $INTERFACE is "class/subclass/protocol"
 * Some interesting classes:
 *	8 = mass storage
 *	7 = printer
 *	3 = HID.   3/1/2 = mouse.
 *	6 = still image (6/1/1 = Digital camera Camera)
 *	9 = Hub
 *	255 = scanner (255/255/255)
 *
 * Observed:
 *	Hub seems to have no INTERFACE (null), and TYPE of "9/0/0"
 *	Flash disk seems to have INTERFACE of "8/6/80", and TYPE of "0/0/0"
 *
 * When a hub is unplugged, a hotplug event is generated for it and everything
 * downstream from it.  You cannot depend on getting these events in any
 * particular order, since there will be many hotplug programs all fired off
 * at almost the same time.
 * On a remove, don't try to access the downstream devices right away, give the
 * kernel time to finish cleaning up all the data structures, which will be
 * in the process of being torn down.
 *
 * On the initial plugin, the first time the kernel usb-storage subsystem sees
 * the host (identified by GUID), it automatically reads the partition table.
 * On subsequent plugins, it does not.
 *
 * Special values for Web Administration to unmount or remount
 * all partitions of the host:
 *	INTERFACE=TOMATO/...
 *	ACTION=add/remove
 *	SCSI_HOST=<host_no>
 * If host_no is negative, we unmount all partions of *all* hosts.
 */
void hotplug_usb(void)
{
	int add;
	int host = -1;
	char *interface = getenv("INTERFACE");
	char *action = getenv("ACTION");
	char *product = getenv("PRODUCT");
#ifdef LINUX26
	char *device = getenv("DEVICENAME");
	int is_block = strcmp(getenv("SUBSYSTEM") ? : "", "block") == 0;
#else
	char *device = getenv("DEVICE");
#endif
	char *scsi_host = getenv("SCSI_HOST");

	_dprintf("%s hotplug INTERFACE=%s ACTION=%s PRODUCT=%s HOST=%s DEVICE=%s\n",
		getenv("SUBSYSTEM") ? : "USB", interface, action, product, scsi_host, device);

	if (!nvram_get_int("usb_enable")) return;
#ifdef LINUX26
	if (!action || ((!interface || !product) && !is_block))
#else
	if (!interface || !action || !product)	/* Hubs bail out here. */
#endif
		return;

	if (scsi_host)
		host = atoi(scsi_host);

	if (!wait_action_idle(10)) return;

	add = (strcmp(action, "add") == 0);
	if (add && (strncmp(interface ? : "", "TOMATO/", 7) != 0)) {
#ifdef LINUX26
		if (!is_block && device)
#endif
		syslog(LOG_DEBUG, "Attached USB device %s [INTERFACE=%s PRODUCT=%s]",
			device, interface, product);


#ifndef LINUX26
		/* To allow automount to be blocked on startup.
		 * In kernel 2.6 we still need to serialize mount/umount calls -
		 * so the lock is down below in the "block" hotplug processing.
		 */
		file_unlock(file_lock("usb"));
#endif
	}

	if (strncmp(interface ? : "", "TOMATO/", 7) == 0) {	/* web admin */
		if (scsi_host == NULL)
			host = atoi(product);	// for backward compatibility
		/* If host is negative, unmount all partitions of *all* hosts.
		 * If host == -1, execute "soft" unmount (do not kill NAS apps, no "lazy" umount).
		 * If host == -2, run "hard" unmount, as if the drive is unplugged.
		 * This feature can be used in custom scripts as following:
		 *
		 * # INTERFACE=TOMATO/1 ACTION=remove PRODUCT=-1 SCSI_HOST=-1 hotplug usb
		 *
		 * PRODUCT is required to pass the env variables verification.
		 */
		/* Unmount or remount all partitions of the host. */
		hotplug_usb_storage_device(host < 0 ? -1 : host, add ? -1 : 0,
			host == -2 ? 0 : EFH_USER);
	}
#ifdef LINUX26
	else if (is_block && strcmp(getenv("MAJOR") ? : "", "8") == 0 && strcmp(getenv("PHYSDEVBUS") ? : "", "scsi") == 0) {
		/* scsi partition */
		char devname[64];
		int lock;

		sprintf(devname, "/dev/%s", device);
		lock = file_lock("usb");
		if (add) {
			if (nvram_get_int("usb_storage") && nvram_get_int("usb_automount")) {
				int minor = atoi(getenv("MINOR") ? : "0");
				if ((minor % 16) == 0 && !is_no_partition(device)) {
					/* This is a disc, and not a "no-partition" device,
					 * like APPLE iPOD shuffle. We can't mount it.
					 */
					return;
				}
				TRACE_PT(" mount to dev: %s\n", devname);
				if (mount_partition(devname, host, NULL, device, EFH_HP_ADD)) {
_dprintf("restart_nas_services(%d): test 5.\n", getpid());
					//restart_nas_services(1, 1); // restart all NAS applications
					notify_rc_after_wait("restart_nasapps");
				}
				TRACE_PT(" end of mount\n");
			}
		}
		else {
			/* When unplugged, unmount the device even if usb storage is disabled in the GUI */
			umount_partition(devname, host, NULL, device, EFH_HP_REMOVE);
			/* Restart NAS applications (they could be killed by umount_mountpoint),
			 * or just re-read the configuration.
			 */
_dprintf("restart_nas_services(%d): test 6.\n", getpid());
			//restart_nas_services(1, 1);
			notify_rc_after_wait("restart_nasapps");
		}
		file_unlock(lock);
	}
#endif
	else if (strncmp(interface ? : "", "8/", 2) == 0) {	/* usb storage */
		run_nvscript("script_usbhotplug", NULL, 2);
#ifndef LINUX26
		hotplug_usb_storage_device(host, add, (add ? EFH_HP_ADD : EFH_HP_REMOVE) | (host < 0 ? EFH_HUNKNOWN : 0));
#endif
	}
	else {	/* It's some other type of USB device, not storage. */
#ifdef LINUX26
		if (is_block) return;
#endif
		/* Do nothing.  The user's hotplug script must do it all. */
		run_nvscript("script_usbhotplug", NULL, 2);
	}
}



// -----------------------------------------------------------------------------

// !!TB - FTP Server

#ifdef RTCONFIG_FTP
static char *get_full_storage_path(char *val)
{
	static char buf[128];
	int len;

	if (val[0] == '/')
		len = sprintf(buf, "%s", val);
	else
		len = sprintf(buf, "%s/%s", MOUNT_ROOT, val);

	if (len > 1 && buf[len - 1] == '/')
		buf[len - 1] = 0;

	return buf;
}

static char *nvram_storage_path(char *var)
{
	char *val = nvram_safe_get(var);
	return get_full_storage_path(val);
}

/* VSFTPD code mostly stolen from Oleg's ASUS Custom Firmware GPL sources */

void write_ftpd_conf()
{
	FILE *fp;
	char maxuser[16];

	/* write /etc/vsftpd.conf */
	fp=fopen("/etc/vsftpd.conf", "w");
	if (fp==NULL) return;

	if (nvram_match("st_ftp_mode", "2"))
		fprintf(fp, "anonymous_enable=NO\n");
	else{
		fprintf(fp, "anonymous_enable=YES\n");
		fprintf(fp, "anon_upload_enable=YES\n");
		fprintf(fp, "anon_mkdir_write_enable=YES\n");
		fprintf(fp, "anon_other_write_enable=YES\n");
	}

	fprintf(fp, "nopriv_user=root\n");
	fprintf(fp, "write_enable=YES\n");
	fprintf(fp, "local_enable=YES\n");
	fprintf(fp, "chroot_local_user=YES\n");
	fprintf(fp, "local_umask=000\n");
	fprintf(fp, "dirmessage_enable=NO\n");
	fprintf(fp, "xferlog_enable=NO\n");
	fprintf(fp, "syslog_enable=NO\n");
	fprintf(fp, "connect_from_port_20=YES\n");
//	fprintf(fp, "listen=YES\n");
	fprintf(fp, "listen%s=YES\n",
#ifdef RTCONFIG_IPV6
	ipv6_enabled() ? "_ipv6" : "");
#else
	"");
#endif
	fprintf(fp, "pasv_enable=YES\n");
	fprintf(fp, "ssl_enable=NO\n");
	fprintf(fp, "tcp_wrappers=NO\n");
	strcpy(maxuser, nvram_safe_get("st_max_user"));
	if ((atoi(maxuser)) > 0)
		fprintf(fp, "max_clients=%s\n", maxuser);
	else
		fprintf(fp, "max_clients=%s\n", "10");
	fprintf(fp, "ftp_username=anonymous\n");
	fprintf(fp, "ftpd_banner=Welcome to ASUS %s FTP service.\n", nvram_safe_get("productid"));

	// update codepage
	modprobe_r("nls_cp936");
	modprobe_r("nls_cp950");

	if (strcmp(nvram_safe_get("ftp_lang"), "EN") != 0)
	{
		fprintf(fp, "enable_iconv=YES\n");
		if (nvram_match("ftp_lang", "TW")) {
			fprintf(fp, "remote_charset=cp950\n");		
			modprobe("nls_cp950");	
		}
		else if (nvram_match("ftp_lang", "CN")) {
			fprintf(fp, "remote_charset=cp936\n");	
			modprobe("nls_cp936");
		}
	}
	fclose(fp);
}

/*
 * st_ftp_modex: 0:no-ftp, 1:anonymous, 2:account 
 */

void
start_ftpd()
{
	if(getpid()!=1) {
		notify_rc_after_wait("start_ftpd");
		return;
	}

	if (nvram_match("enable_ftp", "0")) return;

	write_ftpd_conf();
	
	killall("vsftpd", SIGHUP);
	
	if (!pids("vsftpd"))
		system("vsftpd /etc/vsftpd.conf &");

	if (pids("vsftpd"))
		logmessage("FTP server", "daemon is started");
}

void stop_ftpd(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_ftpd");
		return;
	}

	killall_tk("vsftpd");
	unlink("/tmp/vsftpd.conf");
	logmessage("FTP Server", "daemon is stoped");
}
#endif	// RTCONFIG_FTP

// -----------------------------------------------------------------------------

// !!TB - Samba

#ifdef RTCONFIG_SAMBASRV
void create_custom_passwd()
{
	FILE *fp;
	int result, n=0, i;
	int acc_num;
	char **account_list;

	/* write /etc/passwd.custom */
	fp = fopen("/etc/passwd.custom", "w+");
	result = get_account_list(&acc_num, &account_list);
	if (result < 0) {
		usb_dbg("Can't get the account list.\n");
		free_2_dimension_list(&acc_num, &account_list);
		return;
	}
	for (i=0, n=500; i<acc_num; i++, n++)
	{
		fprintf(fp, "%s:x:%d:%d:::\n", account_list[i], n, n);
	}
	fclose(fp);

	/* write /etc/group.custom  */
	fp = fopen("/etc/group.custom", "w+");
	for (i=0, n=500; i<acc_num; i++, n++)
	{
		fprintf(fp, "%s:x:%d:\n", account_list[i], n);
	}
	fclose(fp);
	free_2_dimension_list(&acc_num, &account_list);
}

static void kill_samba(int sig)
{
	if (sig == SIGTERM) {
		killall_tk("smbd");
		killall_tk("nmbd");
	}
	else {
		killall("smbd", sig);
		killall("nmbd", sig);
	}
}

#ifdef LINUX26
void enable_gro()
{
	char *argv[3] = {"echo", "", NULL};
	char lan_ifname[32], *lan_ifnames, *next;
	char path[64] = {0};

	/* enabled gso on vlan interface */
	lan_ifnames = nvram_safe_get("lan_ifnames");
	foreach(lan_ifname, lan_ifnames, next) {
		if (!strncmp(lan_ifname, "vlan", 4)) {
			sprintf(path, ">>/proc/net/vlan/%s", lan_ifname);
			argv[1] = "-gro 2";
			_eval(argv, path, 0, NULL);
		}
	}
}
#endif

void
start_samba(void)
{
	int acc_num, i;
	char cmd[256];
	char *nv, *nvp, *b;
	char *tmp_account, *tmp_passwd;

	if (getpid() != 1) {
		notify_rc_after_wait("start_samba");
		return;
	}

	if (nvram_match("enable_samba", "0")) return;

#ifdef RTCONFIG_GROCTRL
	enable_gro();
#endif
	
	mkdir_if_none("/var/run/samba");
	mkdir_if_none("/etc/samba");
	
	unlink("/etc/smb.conf");
	unlink("/etc/smbpasswd");

	/* write samba configure file*/
	system("/sbin/write_smb_conf");

	/* write smbpasswd  */
	system("smbpasswd nobody \"\"");

	acc_num = atoi(nvram_safe_get("acc_num"));
	if(acc_num < 0)
		acc_num = 0;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_account, &tmp_passwd) != 2)
				continue;

			sprintf(cmd, "smbpasswd %s %s", tmp_account, tmp_passwd);
_dprintf("%s: cmd=%s.\n", __FUNCTION__, cmd);
			system(cmd);

			if(++i >= acc_num)
				break;
		}
	}
	if(nv)
		free(nv);

	xstart("nmbd", "-D", "-s", "/etc/smb.conf");
	xstart("smbd", "-D", "-s", "/etc/smb.conf");

	logmessage("Samba Server", "daemon is started");

	return;
}

void stop_samba(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_samba");
		return;
	}

	kill_samba(SIGTERM);
	/* clean up */
	unlink("/var/log/smb");
	unlink("/var/log/nmb");
	
	eval("rm", "-rf", "/var/run/samba");

	logmessage("Samba Server", "smb daemon is stoped");

}
#endif	// RTCONFIG_SAMBASRV

#ifdef RTCONFIG_MEDIA_SERVER
#define MEDIA_SERVER_APP	"minidlna"

void start_dms(void)
{
	FILE *f;
	int port, pid;
	char dbdir[100], *dmsdir;
#if 0
	char *argv[] = { MEDIA_SERVER_APP, "-f", "/etc/"MEDIA_SERVER_APP".conf", "-R", NULL };
#else
	char *argv[] = { MEDIA_SERVER_APP, "-f", "/etc/"MEDIA_SERVER_APP".conf", NULL };
#endif
	static int once = 1;
	int i;
	char serial[18];

	if (getpid() != 1) {
		notify_rc("start_dms");
		return;
	}

	if(!is_routing_enabled() && !is_lan_connected())
		set_invoke_later(INVOKELATER_DMS);

	if (nvram_get_int("dms_sas") == 0)
		once = 0;

	if (nvram_get_int("dms_enable") != 0) {

		if ((!once) && (nvram_get_int("dms_rescan") == 0)) {
			// no forced rescan
			argv[3] = NULL;
		}

		if ((f = fopen(argv[2], "w")) != NULL) {
			port = nvram_get_int("dms_port");
			// dir rule:
			// default: dmsdir=/tmp/mnt, dbdir=/var/cache/minidlna
			// after setting dmsdir, dbdir="dmsdir"/minidlna

			dmsdir = nvram_safe_get("dms_dir");
			if(!check_if_dir_exist(dmsdir)) 
				dmsdir = nvram_default_get("dms_dir");
			
			if(strcmp(dmsdir, nvram_default_get("dms_dir"))==0)
				strcpy(dbdir, "/var/cache/minidlna");
			else {
				if(dmsdir[strlen(dmsdir)-1]=='/') sprintf(dbdir, "%sminidlna", dmsdir);
				else sprintf(dbdir, "%s/minidlna", dmsdir);
			}
			mkdir_if_none(dbdir);

			nvram_set("dms_dbcwd", dbdir);

			strcpy(serial, nvram_safe_get("lan_hwaddr"));
			if (strlen(serial))
				for (i = 0; i < strlen(serial); i++)
					serial[i] = tolower(serial[i]);

			fprintf(f,
				"network_interface=%s\n"
				"port=%d\n"
				"friendly_name=%s\n"
//				"db_dir=%s/.db\n"
				"db_dir=%s\n"
				"enable_tivo=%s\n"
				"strict_dlna=%s\n"
				"presentation_url=http://%s:80/\n"
				"inotify=yes\n"
				"notify_interval=600\n"
				"album_art_names=Cover.jpg/cover.jpg/Thumb.jpg/thumb.jpg\n"
				"media_dir=%s\n"
				"serial=%s\n"
				"model_number=1"
				"\n",
				nvram_safe_get("lan_ifname"),
				(port < 0) || (port >= 0xffff) ? 0 : port,
				nvram_get("computer_name") && is_valid_hostname(nvram_get("computer_name")) ? nvram_get("computer_name") : nvram_safe_get("productid"),
				dbdir,
				nvram_get_int("dms_tivo") ? "yes" : "no",
				nvram_get_int("dms_stdlna") ? "yes" : "no",
				nvram_safe_get("lan_ipaddr"),
				dmsdir,
				serial
				);

			fclose(f);
		}

		/* start media server if it's not already running */
		if (pidof(MEDIA_SERVER_APP) <= 0) {
			if ((_eval(argv, NULL, 0, &pid) == 0) && (once)) {
				/* If we started the media server successfully, wait 1 sec
				 * to let it die if it can't open the database file.
				 * If it's still alive after that, assume it's running and
				 * disable forced once-after-reboot rescan.
				 */
				sleep(1);
				if (pidof(MEDIA_SERVER_APP) > 0)
					once = 0;
			}
		}
	}
	else killall_tk(MEDIA_SERVER_APP);
}

void stop_dms(void)
{
	if (getpid() != 1) {
		notify_rc("stop_dms");
		return;
	}

	// keep dms always run except for disabling it
	// killall_tk(MEDIA_SERVER_APP);
}


// it is called by rc only
void force_stop_dms(void)
{
	killall_tk(MEDIA_SERVER_APP);
}


void
write_mt_daapd_conf(char *servername)
{
	FILE *fp;
	char *dmsdir;

	if (check_if_file_exist("/etc/mt-daapd.conf"))
		return;

	fp = fopen("/etc/mt-daapd.conf", "w");

	if (fp == NULL)
		return;

	dmsdir = nvram_safe_get("dms_dir");
	if(!check_if_dir_exist(dmsdir)) 
		dmsdir = nvram_default_get("dms_dir");
#if 1
	fprintf(fp, "web_root /etc/web\n");
	fprintf(fp, "port 3689\n");
	fprintf(fp, "admin_pw %s\n", nvram_safe_get("http_passwd"));
	fprintf(fp, "db_dir /var/cache/mt-daapd\n");
	fprintf(fp, "mp3_dir %s\n", dmsdir);
	fprintf(fp, "servername %s\n", servername);
	fprintf(fp, "runas %s\n", nvram_safe_get("http_username"));
	fprintf(fp, "extensions .mp3,.m4a,.m4p,.aac,.ogg\n");
	fprintf(fp, "rescan_interval 300\n");
	fprintf(fp, "always_scan 1\n");
	fprintf(fp, "compress 1\n");
#else
	fprintf(fp, "[general]\n");
	fprintf(fp, "web_root = /rom/mt-daapd/admin-root\n");
	fprintf(fp, "port = 3689\n");
	fprintf(fp, "admin_pw = %s\n", nvram_safe_get("http_passwd"));
	fprintf(fp, "db_type = sqlite3\n");
	fprintf(fp, "db_parms = /var/cache/mt-daapd\n");
	fprintf(fp, "mp3_dir = %s\n", "/mnt");
	fprintf(fp, "servername = %s\n", servername);
	fprintf(fp, "runas = %s\n", nvram_safe_get("http_username"));
	fprintf(fp, "extensions = .mp3,.m4a,.m4p\n");
	fprintf(fp, "scan_type = 2\n");
	fprintf(fp, "[plugins]\n");
	fprintf(fp, "plugin_dir = /rom/mt-daapd/plugins\n");
	fprintf(fp, "[scanning]\n");
	fprintf(fp, "process_playlists = 1\n");
	fprintf(fp, "process_itunes = 1\n");
	fprintf(fp, "process_m3u = 1\n");
#endif
	fclose(fp);
}

void
start_mt_daapd()
{
	char servername[32];

	if(getpid()!=1) {
		notify_rc("start_mt_daapd");
		return;
	}

	if (nvram_invmatch("daapd_enable", "1"))
		return;

	if (is_valid_hostname(nvram_safe_get("computer_name")))
		strncpy(servername, nvram_safe_get("computer_name"), sizeof(servername));
	else
		servername[0] = '\0';
	if(strlen(servername)==0) strncpy(servername, nvram_safe_get("productid"), sizeof(servername));
	write_mt_daapd_conf(servername);

	if (is_routing_enabled())
	{
		system("mt-daapd -m");
		doSystem("mDNSResponder %s thehost %s _daap._tcp. 3689 &", nvram_safe_get("lan_ipaddr"), servername);
	}
	else
		system("mt-daapd");

	if (pids("mt-daapd"))
	{
		logmessage("iTunes Server", "daemon is started");

		return;
	}

	return;
}

void
stop_mt_daapd()
{
	if(getpid()!=1) {
		notify_rc("stop_mt_daapd");
		return;
	}

	if (!pids("mDNSResponder") && !pids("mt-daapd"))
		return;

	if (pids("mDNSResponder"))
		system("killall mDNSResponder");

	if (pids("mt-daapd"))
		system("killall mt-daapd");

	logmessage("iTunes", "daemon is stoped");
}
#endif
#endif	// RTCONFIG_MEDIA_SERVER

// -------------

// !!TB - webdav

#ifdef RTCONFIG_WEBDAV
void write_webdav_permissions()
{
	FILE *fp;
	int acc_num = 0, i;
	char *nv, *nvp, *b;
	char *tmp_account, *tmp_passwd;

	/* write /tmp/lighttpd/permissions */
	fp = fopen("/tmp/lighttpd/permissions", "w");
	if (fp==NULL) return;

	acc_num = atoi(nvram_safe_get("acc_num"));
	if(acc_num < 0)
		acc_num = 0;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	if(nv && strlen(nv) > 0){
		while((b = strsep(&nvp, "<")) != NULL){
			if(vstrsep(b, ">", &tmp_account, &tmp_passwd) != 2)
				continue;

			fprintf(fp, "%s:%s\n", tmp_account, tmp_passwd);

			if(++i >= acc_num)
				break;
		}
	}
	if(nv)
		free(nv);

	fclose(fp);
}

void write_webdav_server_pem()
{
	unsigned long long sn;
	char t[32];
#ifdef RTCONFIG_HTTPS
	if(f_exists("/etc/server.pem"))
		system("cp -f /etc/server.pem /tmp/lighttpd/");
#endif
	if(!f_exists("/tmp/lighttpd/server.pem")){
		f_read("/dev/urandom", &sn, sizeof(sn));
		sprintf(t, "%llu", sn & 0x7FFFFFFFFFFFFFFFUL);
		eval("gencert.sh", t);

		system("cp -f /etc/server.pem /tmp/lighttpd/");
	}
}

void start_webdav()	// added by Vanic
{
	if(getpid()!=1) {
		notify_rc("start_webdav");
		return;
	}

	if (nvram_match("enable_webdav", "0")) return;

	/* WebDav directory */
	mkdir_if_none("/tmp/lighttpd");
	mkdir_if_none("/tmp/lighttpd/uploads");
	chmod("/tmp/lighttpd/uploads", 0777);
	mkdir_if_none("/tmp/lighttpd/www");
	chmod("/tmp/lighttpd/www", 0777);

	/* tmp/lighttpd/permissions */
	write_webdav_permissions();
	
	/* WebDav SSL support */
	write_webdav_server_pem();

	/* write WebDav configure file*/
	system("/sbin/write_webdav_conf");
	
	if (!f_exists("/tmp/lighttpd.conf")) return;

	if (!pids("lighttpd")){
		system("lighttpd -f /tmp/lighttpd.conf -D &");
	}
	if (!pids("lighttpd-monitor")){
		system("lighttpd-monitor &");
	}

	if (pids("lighttpd"))
		logmessage("WEBDAV server", "daemon is started");
}

void stop_webdav(void)
{
	if (getpid() != 1) {
		notify_rc("stop_webdav");
		return;
	}

	if (pids("lighttpd")){
		killall_tk("lighttpd");
		// charles: unlink lighttpd.conf will cause lighttpd error
		//	we should re-write lighttpd.conf
		system("/sbin/write_webdav_conf");
//		unlink("/tmp/lighttpd.conf");
		//system("killall lighttpd");
	}

	if (pids("lighttpd-monitor")){
		killall_tk("lighttpd-monitor");
	}

	logmessage("WEBDAV Server", "daemon is stoped");
}
#endif	// RTCONFIG_WEBDAV

#ifdef RTCONFIG_USB
void start_nas_services(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("start_nasapps");
		return;
	}

	if(!check_if_dir_empty("/mnt")) return;

	create_passwd();
#ifdef RTCONFIG_SAMBASRV
	start_samba();
#endif

if (nvram_match("asus_mfg", "0")) {
#ifdef RTCONFIG_FTP
	start_ftpd();
#endif
#ifdef RTCONFIG_MEDIA_SERVER
	start_dms();
	start_mt_daapd();
#endif
#ifdef RTCONFIG_WEBDAV
	start_webdav();
#endif
}
}

void stop_nas_services(void)
{
	if (getpid() != 1) {
		notify_rc_after_wait("stop_nasapps");
		return;
	}

#ifdef RTCONFIG_MEDIA_SERVER
	stop_dms();
	stop_mt_daapd();
#endif
#ifdef RTCONFIG_FTP
	stop_ftpd();
#endif
#ifdef RTCONFIG_SAMBASRV
	stop_samba();
#endif
#ifdef RTCONFIG_WEBDAV
	stop_webdav();
#endif
}

void restart_nas_services(int stop, int start)
{
	int fd = file_lock("usbnas");

	/* restart all NAS applications */
	if (stop)
		stop_nas_services();
	if (start)
		start_nas_services();
	file_unlock(fd);
}


void restart_sambaftp(int stop, int start)
{
	int fd = file_lock("sambaftp");
	/* restart all NAS applications */
	if (stop) {
#ifdef RTCONFIG_SAMBASRV
		stop_samba();
#endif
#ifdef RTCONFIG_FTP
		stop_ftpd();
#endif
#ifdef RTCONFIG_WEBDAV
		stop_webdav();
#endif
	}
	
	if (start) {
#ifdef RTCONFIG_SAMBA_SRV
		create_passwd();
		start_samba();
#endif
#ifdef RTCONFIG_FTP
		start_ftpd();
#endif
#ifdef RTCONFIG_WEBDAV
		start_webdav();
#endif
	}
	file_unlock(fd);
}

int ejusb_main(int argc, const char *argv[]){
	disk_info_t *disk_info, *disk_list;
	partition_info_t *partition_info;
	char nvram_name[32], device_name[8], devpath[16];
	int got_usb_port;

	if(argc != 2){
		printf("Usage: ejusb [disk_port]\n");
		return 0;
	}

	got_usb_port = atoi(argv[1]);
	if(got_usb_port < 1 || got_usb_port > 3){
		printf("Usage: ejusb [disk_port]\n");
		return 0;
	}

	disk_list = read_disk_data();
	if(disk_list == NULL){
		printf("Can't get any disk's information.\n");
		return 0;
	}

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d_act", got_usb_port);
	memset(device_name, 0, 8);
	strcpy(device_name, nvram_safe_get(nvram_name));

	for(disk_info = disk_list; disk_info != NULL; disk_info = disk_info->next)
		if(!strcmp(disk_info->device, device_name))
			break;
	if(disk_info == NULL){
		printf("Can't find the information of the device: %s\n", device_name);
		free_disk_data(&disk_list);
		return 0;
	}

	memset(nvram_name, 0, 32);
	sprintf(nvram_name, "usb_path%d_removed", got_usb_port);
	nvram_set(nvram_name, "1");

	for(partition_info = disk_info->partitions; partition_info != NULL; partition_info = partition_info->next){
		if(partition_info->mount_point != NULL){
			memset(devpath, 0, 16);
			sprintf(devpath, "/dev/%s", partition_info->device);

			umount_partition(devpath, 0, NULL, NULL, EFH_HP_REMOVE);
		}
	}
	free_disk_data(&disk_list);

_dprintf("restart_nas_services(%d): test 7.\n", getpid());
	//restart_nas_services(1, 1);
	notify_rc_after_wait("restart_nasapps");

	return 0;
}

#if defined(RTCONFIG_APP_PREINSTALLED) || defined(RTCONFIG_APP_NETINSTALLED)
int start_app(){
	char cmd[PATH_MAX];
	char *apps_dev = nvram_safe_get("apps_dev");
	char *apps_mounted_path = nvram_safe_get("apps_mounted_path");

	if(strlen(apps_dev) <= 0 || strlen(apps_mounted_path) <= 0)
		return -1;

	memset(cmd, 0, PATH_MAX);
	sprintf(cmd, "/opt/.asusrouter %s %s", apps_dev, apps_mounted_path);
	system(cmd);

	return 0;
}

int stop_app(){
	char *apps_dev = nvram_safe_get("apps_dev");
	char *apps_mounted_path = nvram_safe_get("apps_mounted_path");

	if(strlen(apps_dev) <= 0 || strlen(apps_mounted_path) <= 0)
		return -1;

	system("app_stop.sh");
	sync();

	return 0;
}
#endif
#endif // RTCONFIG_USB

#ifdef RTCONFIG_WEBDAV
#define DEFAULT_WEBDAVPROXY_RIGHT 0

int find_webdav_right(char *account)
{
	char *nv, *nvp, *b;
	char *acc, *right;
	int ret;

	nv = nvp = strdup(nvram_safe_get("acc_webdavproxy"));
	ret = DEFAULT_WEBDAVPROXY_RIGHT;

	if(nv) {
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &acc, &right) != 2)) continue;

			if(strcmp(acc, account)==0) {
				ret = atoi(right);
				break;
			}
		}
		free(nv);
	}

	return ret;
}

void webdav_account_default(void)
{
	char *nv, *nvp, *b;
	char *accname, *accpasswd;
	int right;
	char new[256];
	int i;

	nv = nvp = strdup(nvram_safe_get("acc_list"));
	i = 0;
	strcpy(new, "");

	if(nv) {
		i=0;
		while ((b = strsep(&nvp, "<")) != NULL) {
			if((vstrsep(b, ">", &accname, &accpasswd) != 2)) continue;

			right = find_webdav_right(accname);
				
			if(i==0) sprintf(new, "%s>%d", accname, right);
			else sprintf(new, "%s<%s>%d", new, accname, right);
			i++;
		}
		free(nv);
		nvram_set("acc_webdavproxy", new);
	}
}
#endif

