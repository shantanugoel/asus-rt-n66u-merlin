Asus RT-N66U Modded Firmware - build 3.0.0.3.108.4 (28-April-2012)
==================================================================

About
-----
These builds of the RT-N66U (Rev B1) firmware are versions I have modified and 
recompiled.  The goal is to do some minor improvements to Asus' firmware, 
without targeting at full-blown featuresets such as provided by excellent 
projects such as Tomato or DD-WRT.  This aims to be a more restrained 
alternative for those who prefer to stay closer to the original firmware.

The list of changes (so far):

- Updated MiniDLNA from 1.0.21 to 1.0.24 (all Asus patches to the 
  MiniDLNA sources were reapplied).  
  MiniDLNA Changelog: http://sourceforge.net/projects/minidlna/files/minidlna/1.0.24/
- Added wol binary (wake-on-lan) (in addition to ether-wake already in the firmware)
- Added Tools menu to web interface (with WakeOnLan page)
- Added JFFS partition support (configurable under Administration->Advanced->System)
- Added user scripts that run on specific events
- Added SSHD (dropbear, configurable under Administration->Advanced->System)
- Clicking on the MAC address of an unidentified client will do a lookup in
  the OUI database (ported from DD-WRT).
- Enabled HTTPS access to web interface
- Optionally turn the WPS button into a radio enable/disable switch


Installation
------------
Simply flash it like any regular update.  You should not need to reset to 
factory defaults, unless coming from a newer or a buggy version of Asus' 
original firmware.  You can revert back at any time by re-flashing an 
original Asus firmware.



Usage
-----

*JFFS*
JFFS is a writable section of the flash memory (around 12 MB) which will 
allow you to store small files (such as scripts) inside the router without 
needing to have a USB disk plugged in.  This space will survive reboot (but 
it *MIGHT NOT survive firmware flashing*, so back it up first before flashing!).  
It will also be available fairly early at boot (unlike a USB disk).

To enable this option, go to the Administration page, under the System tab.

First time you enable JFFS, it must be formatted.  This can be done through 
the web page, same page where you enable it.  Enabling/Disabling/Formating 
JFFS requires a reboot to take effect.


*User scripts:*
These are shell scripts that you can create, and which will be run when 
certain events occur:

- Services are started (boot): /jffs/scripts/services-start
- Services are stopped (reboot): /jffs/scripts/services-stop
- WAN interface comes up (includes if it went down and 
  back up): /jffs/scripts/wan-start
- Firewall is started (rules are applied): /jffs/scripts/firewall-start.

Those scripts must all be located under /jffs/scripts/ (so JFFS support 
must be enabled first).

The wan-start script for example is perfect for using an IPv6 tunnel 
update script (such as the script I posted on my website to use with 
HE's TunnelBroker).  You could then put your IPv6 rules inside 
firewall-start.


* WakeOnLan *
There's a new Tools section on the web interface.  From there you can enter a 
target computer's MAC address to send it a WakeOnLan packet.  There is also a list 
of recently seen clients on that page - you can click on an entry to automatically 
enter it as your target.


* SSHD *
SSH support (through Dropbear) was re-enabled.  Password-based login will use 
the "admin" username (like telnet), and the same password as used to log on  
the web interface.  You can also optionally insert a RSA public key there 
for keypair-based authentication.  Note that to ensure you do not accidentally 
enter too much data there (and potentially filling the already overcrowded 
NVRAM space - what the hell was Asus thinking when they went with 32KB?), 
I am limiting this field to 512 characters max.


* HTTPS management *
I re-enabled HTTPS access in the firmware.  From the Administration->System 
page you can configure your router so it accepts connections on http, https 
or both.  You can also change the https port to a different one 
(default is 8443).


* WPS button mode - toggle radio *
You can configure the router so pressing the WPS button will 
toggle the radio on/off instead of starting WPS mode.
The option to enable this feature can be found on the 
Administration page, under the System tab.



Notes
-----
To make it simple to determine which version you are running, I am simply 
appending another digit to determine my build version.  
For example, 3.0.0.3.108 becomes 3.0.0.3.108.1.



Source code
-----------
The source code with all my modifications can be found 
on Github, at:

https://github.com/shantanugoel/asus-rt-n66u-merlin/tree/merlin

The "merlin" branch contains my modifications to the Asus firmware.


History
-------

3.0.0.3.108.4:
   - NEW: Clicking on the MAC address of an unidentified client will do a lookup in
          the OUI database (ported from DD-WRT).
   - NEW: Added HTTPS access to web interface (configurable under Administration)
   - NEW: Option to turn the WPS button into a radio on/off toggle (under Administration)
   - FIXED: sshd would start even if disabled
   - CHANGE: Switched back to wol, as people report better compatibility with it.
             ether-wake remains available over Telnet.


3.0.0.3.108.3:
   - NEW: JFFS support (mounted under /jffs)
   - NEW: services-start, services-stop, wan-start and firewall-start user scripts,
          must be located in /jffs/scripts/ .
   - NEW: SSHD support
   - IMPROVED: Fleshed out this documentation, updated Contact info with SNB forum URL
   - CHANGE: Removed wol binary, and switched to ether-wake (from busybox) instead.
   - CHANGE: Added "Merlin build" next to the firmware version on web interface.
   
          
3.0.0.3.108.2:
   - NEW: Added WakeOnLan web page

   
3.0.0.3.108.1:
   - Initial release.
   
   
Contact information
-------------------
SmallNetBuilder forums (preferred method: http://forums.smallnetbuilder.com/showthread.php?t=7047 as RMerlin)
Asus Forums (http://vip.asus.com/forum/topic.aspx?board_id=11&model=RT-N66U%20(VER.B1)&SLanguage=en-us) as RMerlin.
Website: http://www.lostrealm.ca/
Email: rmerl@lostrealm.ca

Drop me a note if you are using this firmware and are enjoying it.  If you really like it and want 
to give more than a simple "Thank you", there is also a Paypal donation button on my website.  I 
wouldn't mind being able to afford a second RT-N66U, so I can work on this firmware without 
constantly cutting my Internet access :)


--- 
Eric Sauvageau
