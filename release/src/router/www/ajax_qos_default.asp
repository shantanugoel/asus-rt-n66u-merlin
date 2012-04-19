<?xml version="1.0" ?>
<qos_default_list>
<qos_rule><desc>WWW, SSL, HTTP Proxy</desc><port>80,443,8080</port><proto>TCP</proto><rate>0~512</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>HTTP, SSL, File transfers</desc><port>80,443,8080</port><proto>TCP/UDP</proto><rate>~512</rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>DNS, Time, NTP, RSVP</desc><port>53,37,123,3455</port><proto>TCP/UDP</proto><rate>0~10</rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Xbox Live</desc><port>88, 3074</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Playstation network</desc><port>10070:10080</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Halo</desc><port>2302,2303</port><proto>UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>World of Warcraft</desc><port>3724, 6112,6881:6999</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Starcraft</desc><port>6112:6119, 4000</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Entropia Universe</desc><port>554,30583,30584</port><proto>TCP/UDP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Runescape</desc><port>43594,43595</port><proto>TCP</proto><rate></rate><prio>Highest</prio></qos_rule>
<qos_rule><desc>Netmeeting(TCP)</desc><port>389,522,1503,1720,1731</port><proto>TCP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>PPTV</desc><port>4004,8008</port><proto>TCP/UDP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>FaceTime(TCP)</desc><port>443,5223</port><proto>TCP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>FaceTime(UDP)</desc><port>3478:3497, 16384:16387, 16393:16402</port><proto>UDP</proto><rate></rate><prio>High</prio></qos_rule>
<qos_rule><desc>SMTP, POP3, IMAP, NNTP</desc><port>25,465,563,587,110,119,143,220,993,995</port><proto>TCP/UDP</proto><rate></rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>Windows Live</desc><port>1493,1502,1503,1542,1863,1963,3389,506,5190:5193,7001</port><proto></proto><rate></rate><prio>Medium</prio></qos_rule>
<qos_rule><desc>FTP,SFTP,WLM,File/WebCam</desc><port>20:23, 6571, 681:6901</port><proto>TCP/UDP</proto><rate></rate><prio>Low</prio></qos_rule>
<qos_rule><desc>eDonkey</desc><port>4661:4662,4665</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>eMule</desc><port>4661:4662,4665,4672,4711</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>Shareaza</desc><port>6346</port><proto>TCP/UDP</proto><rate></rate><prio>Lowest</prio></qos_rule>
<qos_rule><desc>BitTorrent</desc><port>6881:6889</port><proto>TCP</proto><rate></rate><prio>Lowest</prio></qos_rule>
</qos_default_list>