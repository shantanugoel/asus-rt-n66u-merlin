<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>AiDisk Wizard</title>
<link rel="stylesheet" type="text/css" href="/NM_style.css">
<link rel="stylesheet" type="text/css" href="aidisk.css">
<link rel="stylesheet" type="text/css" href="/form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script>
var FTP_status = parent.get_ftp_status();  // FTP  0=disable 1=enable
var FTP_mode = parent.get_share_management_status("ftp");  // if share by account. 1=no 2=yes
var Webdav_status = '<% nvram_get("enable_webdav"); %>';
var isSubmit = 0;
var isNext = 0;

function initial(){
	showdisklink();
	if(Webdav_status == 0)
		$("closeBtn").style.visibility = 'hidden';
}

// webdav
function resultOfSwitchWebdavStatus(){
	if(isNext == 1)
		location.href = "/cloud/Aidisk-2.asp";
	else
		location.href = "/cloud/Aidisk-1.asp";
}

// FTP
function switchAppStatus(_app, _status){  // turn on/off the share
	document.APPForm.protocol.value = _app;
	document.APPForm.flag.value = _status;
	document.APPForm.submit();
}

function resultOfSwitchAppStatus(){
}

// if disk
function showdisklink(){
	if(detect_mount_status() == 0){ // No USB disk plug.
		$("AiDiskWelcome_desp").style.display = 'none';
		$("linkdiskbox").style.display = 'none';
		$("Nodisk_hint").style.display = 'block';
		$("gotonext").style.display = 'none';
		return;
	}
}

function detect_mount_status(){
	var mount_num = 0;
	for(var i = 0; i < parent.foreign_disk_total_mounted_number().length; ++i)
		mount_num += parent.foreign_disk_total_mounted_number()[i];
	return mount_num;
}

function go_next_page(){
	isNext++;

	if(Webdav_status == 0){
		$("cloud_item_webdav_loading").style.display = "";
		switchAppStatus("webdav", "on");
	}
	else
		location.href = "/cloud/Aidisk-2.asp";
}

function disable_webdav(){
	$("cloud_item_webdav_loading").style.display = "";
	switchAppStatus("webdav", "off");
}
</script>
</head>
<body onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="GET" name="redirectForm" action="">
<input type="hidden" name="flag" value="">
</form>

<form method="post" name="APPForm" action="/cloud/switch_AiDisk_app.asp" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="protocol" id="protocol" value="ftp">
<input type="hidden" name="mode" id="mode" value="account">
<input type="hidden" name="flag" id="flag" value="">
<input type="hidden" name="account" id="account" value="">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
<input type="hidden" name="permission" id="permission" value="">
</form>

<div class="aidisk1_table">
<table>
<tr>
  <td valign="top" bgcolor="#4d595d" style="padding-top:25px;">
  <table width="740" height="125" border="0">
  	<tr>
    	<td>
    	<table width="740" border="0">
    		<tr>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step1.png) 0% 95% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step2.png) 0% 0% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step3.png) 0% 0% no-repeat;"></td>
    			<td width="25%">&nbsp;</td>
    		</tr>
    	</table>
    	</td>
    </tr>

		<tr>
			<td align="center">
				<br>
				<br>
				<br>
				<br>
				<br>
				<br>
				<span><strong class="cloud_item">Click Next to enable WebDav.</strong></span>
		  </td>
	  </tr>
	   
		<tr style="display:none">
			<td align="center">
				<table style="margin-top:120px;">
					<tr>
						<td>
    					<div id="ftp" onclick="switchAppStatus('ftp', 'on');"></div>
    				</td>
						<td>
    					<div style="font-style: italic;">ftp desc</div>
    				</td>
						<td width="50px">&nbsp;</td>
						<td>
    					<div id="webdav" onclick="switchAppStatus('webdav', 'on');"></div>
						</td>
						<td>
    					<div style="font-style: italic;">webdav desc</div>
    				</td>
					</tr>
					<tr>
						<td align="center">
    					<span><strong class="cloud_item">FTP</strong></span>
							<img id="cloud_item_ftp_loading" style="margin-left:5px;display:none;" src="/images/InternetScan.gif" />
    				</td>
						<td>
    					<div></div>
    				</td>
						<td>&nbsp;</td>
						<td align="center">
    					<span><strong class="cloud_item">WebDav</strong></span>
						</td>
						<td>
    					<div></div>
    				</td>
					</tr>
				</table>
		  </td>
	  </tr>
	      
	  <tr>
	  	<td align="center" width="740px" height="60px">
				<br>
				<br>
				<br>
				<br>
				<br>
				<br>
				<div id="gotonext">
		    	<a id="closeBtn" href="javascript:disable_webdav();"><div class="titlebtn" align="center" style="margin-left:280px;_margin-left:170px;width:80px;"><span><#btn_disable#></span></div></a>
		    	<a href="javascript:go_next_page();"><div class="titlebtn" align="center" style="width:80px;"><span><#CTL_next#></span></div></a>
					<img id="cloud_item_webdav_loading" style="margin-top:5px;margin-left:-250px;display:none;" src="/images/InternetScan.gif" />
				</div>
	  	</td>
	  </tr>
  </table>
  </td>
</tr>  
</table>
</div>
</body>
</html>
