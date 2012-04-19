<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>Untitled Document</title>
<link rel="stylesheet" type="text/css" href="../NM_style.css">
<link rel="stylesheet" type="text/css" href="../form_style.css">
<script type="text/javascript" src="formcontrol.js"></script>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
var $j = jQuery.noConflict();
</script>
<script>
<% wanlink(); %>

var wanip = wanlink_ipaddr();
var wandns = wanlink_dns();
var wangateway = wanlink_gateway();
var wanstate = -1;
var wansbstate = -1;
var wanauxstate = -1;
var old_link_internet = -1;
var lan_proto = '<% nvram_get("lan_proto"); %>';

<% wan_get_parameter(); %>

function initial(){
	flash_button();
	
	if(sw_mode == 1){
		setTimeout("update_wanip();", 1);
	}
	else{
		$("rt_table").style.display = "none";
		$("ap_table").style.display = "";
		if(sw_mode == 2)
			showtext($("RemoteAP"), decodeURIComponent(document.internetForm.wlc_ssid.value));

		if(lan_proto == "static")
			showtext($("LanProto"), "<#BOP_ctype_title5#>");
		else
			showtext($("LanProto"), "<#BOP_ctype_title1#>");
	}

	if(wanlink_type() == "dhcp")
		var wanlink_type_conv = "<#BOP_ctype_title1#>";
	else 	if(wanlink_type() == "pppoe" ||wanlink_type() == "PPPOE")
		var wanlink_type_conv = "PPPoE";
	else 	if(wanlink_type() == "static")
		var wanlink_type_conv = "<#BOP_ctype_title5#>";
	else 	if(wanlink_type() == "pptp")
		var wanlink_type_conv = "PPTP";
	else 	if(wanlink_type() == "l2tp")
		var wanlink_type_conv = "L2TP";
	else
		var wanlink_type_conv = wanlink_type();

	showtext($j("#connectionType")[0], wanlink_type_conv);
	update_all_ip(wanip, wandns, wangateway);
}

function update_all_ip(wanip, wandns, wangateway){
	var dnsArray = wandns.split(" ");
	if(dnsArray[0])
		showtext($j("#DNS1")[0], dnsArray[0]);
	if(dnsArray[1])
		showtext($j("#DNS2")[0], dnsArray[1]);

	showtext($j("#WANIP")[0], wanip);
	showtext($j("#gateway")[0], wangateway);
}

function update_wan_state(state, auxstate){
	if(state == "2" && auxstate == "0")
		link_internet = 1;
	else
		link_internet = 0;
	return link_internet;
}

function update_wanip(e) {
  $j.ajax({
    url: '/status.asp',
    dataType: 'script', 
	
    error: function(xhr) {
      setTimeout("update_wanip();", 3000);
    },
    success: function(response) {
    	wanip = wanlink_ipaddr();
			wandns = wanlink_dns();
			wangateway = wanlink_gateway();
			
			if(old_link_internet == -1)
				old_link_internet = update_wan_state(wanstate, wanauxstate);
			
			if(update_wan_state(wanstate, wanauxstate) != old_link_internet) {
				refreshpage();
			}
			else{
				update_all_ip(wanip, wandns, wangateway);
				setTimeout("update_wanip();", 3000);
			}
    }
  });
}

function submitWANAction(status){
	switch(status){
		case 0:
			parent.showLoading();
			setTimeout('location.href = "/device-map/wan_action.asp?wanaction=Connect";', 1);
			break;
		case 1:
			parent.showLoading();
			setTimeout('location.href = "/device-map/wan_action.asp?wanaction=Disconnect";', 1);
			break;
		default:
			alert("No change!");
	}
}

function goQIS(){
	parent.location.href = '/QIS_wizard.htm';
}
</script>
</head>

<body class="statusbody" onload="initial();">
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="internetForm" id="form" action="/start_apply2.htm">
<input type="hidden" name="current_page" value="/index.asp">
<input type="hidden" name="next_page" value="/index.asp">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_wan_if">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="wan_enable" value="<% nvram_get("wan_enable"); %>">
<input type="hidden" name="wan_unit" value="<% get_wan_unit(); %>">
<input type="hidden" name="wlc_ssid" value="<% nvram_char_to_ascii("WLANConfig11b", "wlc_ssid"); %>" disabled>

<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" id="rt_table">
<tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px;"><#ConnectionStatus#></p>
    		<div class="left" style="width:94px; float:right;" id="radio_wan_enable"></div>
				<div class="clear"></div>
				<script type="text/javascript">
						$j('#radio_wan_enable').iphoneSwitch('<% nvram_get("wan_enable"); %>', 
							 function() {
								document.internetForm.wan_enable.value = "1";
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 },
							 function() {
								document.internetForm.wan_enable.value = "0";
								parent.showLoading();
								document.internetForm.submit();	
								return true;
							 },
							 {
								switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
							 }
						);
				</script>
    		<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
  
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#WAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="WANIP"></p>
    		<span id="wan_status" style="display:none"></span>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" >DNS:</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS1"></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="DNS2"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" ><#Connectiontype#>:</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="connectionType"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm" ><#Gateway#>:</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="gateway"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<!--tr>
    <td height="50" style="padding:10px 15px 0px 15px;">
    		<p class="formfonttitle_nwm" style="float:left;width:98px; "><#QIS#></p>
    		<input type="button" class="button_gen" value="<#btn_go#>" onclick="javascript:goQIS();">
    </td>
</tr-->  
</table>

<table width="95%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="table1px" id="ap_table" style="display:none">
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#statusTitle_AP#> :</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="RemoteAP"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#Connectiontype#> :</p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;" id="LanProto"></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>

<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#WAN_IP#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_ipaddr"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#HSDPAConfig_Subnetmask_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_netmask"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#HSDPAConfig_DefGateway_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_gateway"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
<tr>
    <td style="padding:5px 10px 5px 15px;">
    		<p class="formfonttitle_nwm"><#HSDPAConfig_DNSServers_itemname#></p>
    		<p style="padding-left:10px; margin-top:3px; background-color:#444f53; line-height:20px;"><% nvram_get("lan_dns"); %></p>
      	<img style="margin-top:5px;" src="/images/New_ui/networkmap/linetwo2.png">
    </td>
</tr>
</table>

</form>
<select id="domore" class="input_option_left" onchange="domore_link(this);" style="display:none">
  <option selected="selected"><#MoreConfig#>...</option>
  <option value="../Advanced_WAN_Content.asp"><#menu5_3_1#></option>
  <option value="../Advanced_PortTrigger_Content.asp"><#menu5_3_3#></option>
  <option value="../Advanced_VirtualServer_Content.asp"><#menu5_3_4#></option>
  <option value="../Advanced_Exposed_Content.asp"><#menu5_3_5#></option>  
  <option value="../Advanced_ASUSDDNS_Content.asp"><#menu5_3_6#></option>
  <option value="../Main_IPTStatus_Content.asp"><#menu5_7_5#></option>
</select>
</body>
</html>
