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
var Webdav_status = '<% nvram_get("webdav_proxy"); %>';

var acc_webdavproxy_array = '<% nvram_get("acc_webdavproxy"); %>'.split("&#60");
var acc_webdavproxy = new Array();
for(i=0;i<acc_webdavproxy_array.length;i++){
	acc_webdavproxy.splice(i, 1, acc_webdavproxy_array[i].split("&#62")[1]);
}

function initial(){
}

// webdav
function switchWebdavStatus(){  // turn on/off the share
	if(Webdav_status == '0'){
		$("acc_webdavproxy").disabled = true;
		document.WebdavForm.webdav_proxy.value = 0;
	}
	else{
		if(acc_webdavproxy[0] == 0){
			acc_webdavproxy[0] = 1;
			$("acc_webdavproxy").value = "";
			for(i=0; i<acc_webdavproxy.length; i++){
				$("acc_webdavproxy").value += acc_webdavproxy_array[i].split("&#62")[0] + ">";
				$("acc_webdavproxy").value += acc_webdavproxy[i];
				if(i != acc_webdavproxy.length-1)
					$("acc_webdavproxy").value += "<";
			}
		}
		document.WebdavForm.webdav_proxy.value = 1;
	}

	document.WebdavForm.submit();
}

function resultOfSwitchWebdavStatus(){
	location.href = "/cloud/Aidisk-3.asp";
}

function go_next_page(){
	$("cloud_item_webdav_loading").style.display = "";
	switchWebdavStatus();
}

function go_pre_page(){
	location.href = "/cloud/Aidisk-1.asp";
}
</script>
</head>
<body onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="GET" name="redirectForm" action="">
<input type="hidden" name="flag" value="">
</form>

<form method="post" name="FTPForm" action="/cloud/switch_AiDisk_app.asp" target="hidden_frame">
<input type="hidden" name="motion" id="motion" value="">
<input type="hidden" name="layer_order" id="layer_order" value="">
<input type="hidden" name="protocol" id="protocol" value="ftp">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="flag" id="flag" value="">
<input type="hidden" name="account" id="account" value="">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
<input type="hidden" name="permission" id="permission" value="">
</form>

<form method="post" name="WebdavForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="flag" value="aidisk">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_webdav">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">
<input type="hidden" name="acc_webdavproxy" id="acc_webdavproxy" value="">
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
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step1.png) 0% 0% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step2.png) 0% 95% no-repeat;"></td>
    			<td width="15%"><img src="/images/New_ui/aidisk/steparrow.png" /></td>
    			<td width="15%" height="90px" style="background:url(/images/New_ui/aidisk/step3.png) 0% 0% no-repeat;"></td>
    			<td width="25%">&nbsp;</td>
    		</tr>
    	</table>
    	</td>
    </tr>

		<tr>
			<td align="center" colspan="2">
				<br>
				<br>
				<br>
				<br>
				<br>
				<br>
				<input type="checkbox" id="" class="input" onChange="Webdav_status=(this.value=='on')?1:0" <% nvram_match("webdav_proxy", "1", "checked"); %>>
    		<span><strong class="cloud_item">Enable Samba to WebDav</strong></span>
    	</td>
		</tr>				
	   
		<tr style="display:none">
			<td align="center">
				<table style="margin-top:120px;">
					<tr>
						<td>
    					<div id="cloud_webdav" onclick="switchWebdavStatus();"></div>
						</td>
						<td>
    					<div style="font-style: italic;">smb to webdav desc</div>
    				</td>
					</tr>
					<tr>
						<td align="center" colspan="2">
							<input type="checkbox" id="" class="input" onChange="">	
    					<span><strong class="cloud_item">Samba to WebDav</strong></span>
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
		    	<a href="javascript:go_pre_page();"><div class="titlebtn" align="center" style="margin-left:275px;_margin-left:137px;width:80px;"><span><#btn_pre#></span></div></a>
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
