<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router <#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script>
<% login_state_hook(); %>
<% disk_pool_mapping_info(); %>
<% available_disk_names_and_sizes(); %>
<% get_AiDisk_status(); %>

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var account_num;
//var accounts;
var pools = [];
var folderlist = [];
var page = parseInt('<% get_parameter("page"); %>'-'0');
var ddns_enable_x = '<% nvram_get("ddns_enable_x"); %>';

function initial(){
	show_menu();

	if(page == 2)
		show_iframe("/cloud/Aidisk-2.asp");
	else if(page == 3)
		show_iframe("/cloud/Aidisk-3.asp");
	else
		show_iframe("/cloud/Aidisk-1.asp");
}

function setASUSDDNS_enable(flag){
		this.ddns_enable_x = flag;
}

function show_iframe(src){
	$("sub_frame").src = "";
	setTimeout('$("sub_frame").src = \"'+src+'\";', 1);
}

function show_iframe_page(iframe_id){
	if(iframe_id)
		if($(iframe_id))
			return $(iframe_id).src;
	
	return "";
}
</script>
</head>

<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="applyForm" id="applyForm" action="" target="hidden_frame">
<input type="hidden" name="dummyShareway" id="dummyShareway" value="<% nvram_get("dummyShareway"); %>">
<input type="hidden" name="account" id="account" value="">
<input type="hidden" name="password" id="password" value="">
<input type="hidden" name="protocol" id="protocol" value="">
<input type="hidden" name="mode" id="mode" value="">
<input type="hidden" name="pool" id="pool" value="">
<input type="hidden" name="folder" id="folder" value="">
<input type="hidden" name="permission" id="permission" value="">
<input type="hidden" name="flag" id="flag" value="on">
</form>

<form method="post" name="ddnsForm" id="ddnsForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="ddns_enable_x" value="">
</form>

<form method="post" name="parameterForm" id="parameterForm" action="" target="">
<input type="hidden" name="next_page" id="next_page" value="">
<input type="hidden" name="accountNum" id="accountNum" value="0">
<input type="hidden" name="account1" id="account1" value="">
<input type="hidden" name="passwd1" id="passwd1" value="">
<input type="hidden" name="permission1" id="permission1" value="">
</form>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td height="430" align="center" valign="top">
			<div id="tabMenu"></div>
		<!--==============Beginning of hint content=============-->
			<table width="100%" height="100%" border="0" bgcolor="#4d595d" cellpadding="0"  cellspacing="0" style="margin-top:-145px;-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;">
			  <tr>
					<td align="left" valign="top">
						<iframe id="sub_frame" src="" width="760px" height="710px" frameborder="0" scrolling="no" style="position:relative;-webkit-border-radius:3px;-moz-border-radius:3px;border-radius:3px;"></iframe>
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</body>
</html>
