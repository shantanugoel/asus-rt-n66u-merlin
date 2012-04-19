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
<title><#Web_Title#> - Dual WAN</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
var wans_caps = '<% nvram_get("wans_cap"); %>';
var wans_dualwan = '<% nvram_get("wans_dualwan"); %>';

var wans_caps_primary = wans_caps;
var wans_caps_secondary = Trim(Trim(wans_caps.replace("dsl","")).replace("wan",""));

var $j = jQuery.noConflict();

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]

function initial(){
	show_menu();

  addWANOption(document.form.wans_primary, wans_caps_primary);
  document.form.wans_primary.value = wans_dualwan.split(" ")[0];
  addWANOption(document.form.wans_second, wans_caps_secondary);
  document.form.wans_second.value = wans_dualwan.split(" ")[1];

	if(wans_dualwan.split(" ")[1] != 'none'){
		document.form.wans_primary.parentNode.parentNode.style.display = "";
		document.form.wans_second.parentNode.parentNode.style.display = "";
	}
	else{
		document.form.wans_primary.parentNode.parentNode.style.display = "none";
		document.form.wans_second.parentNode.parentNode.style.display = "none";
	}	

  document.form.wans_lb_ratio_0.value = '<% nvram_get("wans_lb_ratio"); %>'.split(':')[0];
  document.form.wans_lb_ratio_1.value = '<% nvram_get("wans_lb_ratio"); %>'.split(':')[1];
}

function LTrim(str){
    var i;
    for(i=0;i<str.length;i++) {
        if(str.charAt(i)!=" "&&str.charAt(i)!=" ") break;
    }
    str = str.substring(i,str.length);
    return str;
}

function RTrim(str){
    var i;
    for(i=str.length-1;i>=0;i--){
        if(str.charAt(i)!=" "&&str.charAt(i)!=" ") break;
    }
    str = str.substring(0,i+1);
    return str;
}

function Trim(str){
    return LTrim(RTrim(str));
}

function removeOptions(obj) {
	if (obj == null) return;
	if (obj.options == null) return;
    obj.options.length = 0; 
}

function applyRule(){
	if(document.form.wans_dualwan.value.split(' ')[1] != 'none')
		document.form.wans_dualwan.value = document.form.wans_primary.value + " " + document.form.wans_second.value;

	document.form.wans_lb_ratio.value = document.form.wans_lb_ratio_0.value + ":" + document.form.wans_lb_ratio_1.value;
	showLoading();
	document.form.submit();
}

function changeWANProto(_value){
  addWANOption(document.form.wans_second, Trim(wans_caps_secondary.replace(_value,"")));
}

function addWANOption(obj, wanscapItem){
	free_options(obj);

	if(wanscapItem.search(" ") > 0)
		for(i=0; i<wanscapItem.split(" ").length; i++){
			obj.options[i] = new Option(wanscapItem.split(" ")[i].toUpperCase(), wanscapItem.split(" ")[i]);
	}
	else
		obj.options[0] = new Option(wanscapItem.toUpperCase(), wanscapItem);

	appendLANoption();
}

function appendLANoption(){	
	if(typeof(document.form.wans_lan)!="undefined" && document.form.wans_lan.parentNode != null)
  	document.form.wans_lan.parentNode.removeChild(document.form.wans_lan);

	_appendLANoption(document.form.wans_primary);
	_appendLANoption(document.form.wans_second);
}

function _appendLANoption(obj){
	if(obj.value == "lan"){
		var childsel=document.createElement("select");
		childsel.setAttribute("id","wans_lan");
		childsel.setAttribute("name","wans_lan");
		childsel.setAttribute("class","input_option");
		obj.parentNode.appendChild(childsel);
		document.form.wans_lan.options[0] = new Option("LAN Port 1", "lan1");
		document.form.wans_lan.options[1] = new Option("LAN Port 2", "lan2");
		document.form.wans_lan.options[2] = new Option("LAN Port 3", "lan3");
		document.form.wans_lan.options[3] = new Option("LAN Port 4", "lan4");
	}
}
</script>
</head>
<body onload="initial();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_WANPort_Content.asp">
<input type="hidden" name="next_page" value="Advanced_WANPort_Content.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="30">
<input type="hidden" name="action_script" value="reboot">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="wl_ssid" value="<% nvram_get("wl_ssid"); %>">
<input type="hidden" name="wans_dualwan" value="<% nvram_get("wans_dualwan"); %>">
<input type="hidden" name="wans_lb_ratio" value="<% nvram_get("wans_lb_ratio"); %>">
<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>		
		<td valign="top" width="202">				
			<div id="mainMenu"></div>	
			<div id="subMenu"></div>		
		</td>						
    <td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
			
			<!--===================================Beginning of Main Content===========================================-->

			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
				<tr>
					<td valign="top">
						<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
								<tr>
								  <td bgcolor="#4D595D" valign="top">
								  <div>&nbsp;</div>
								  <div class="formfonttitle">Wan Port Setup</div>
								  <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
					  			<div class="formfontdesc"><#Layer3Forwarding_x_ConnectionType_sectiondesc#></div>
									<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
										<tr>
										<th>Enable Dual Wan</th>
											<td>
												<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_dualwan_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$j('#radio_dualwan_enable').iphoneSwitch(document.form.wans_dualwan.value.split(' ')[1] != 'none',
														 function() {
															document.form.wans_second.parentNode.parentNode.style.display = "";
														 },
														 function() {
															document.form.wans_dualwan.value = document.form.wans_primary.value + " none";
															document.form.wans_second.parentNode.parentNode.style.display = "none";
														 },
														 {
															switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
														 }
													);
												</script>			
												</div>	
											</td>
										</tr>

										<tr>
											<th>Primary WAN</th>
											<td>
												<select name="wans_primary" class="input_option" onchange="changeWANProto(this.value);"></select>
											</td>
									  </tr>
										<tr>
											<th>Secondary WAN</th>
											<td id="tdsel">
												<select name="wans_second" class="input_option" onchange="appendLANoption();"></select>
											</td>
									  </tr>

										<tr style="display:none;">
											<th>Multi WAN Mode</th>
											<td>
												<select name="wans_mode" class="input_option">
													<option value="fo" <% nvram_match("wans_mode", "fo", "selected"); %>>Fail Over</option>
													<option value="lb" <% nvram_match("wans_mode", "lb", "selected"); %>>LoadBalance</option>
													<option value="rt" <% nvram_match("wans_mode", "rt", "selected"); %>>Routing</option>													
												</select>			
											</td>
									  </tr>

			          		<tr style="display:none;">
			            		<th>Load Balance Configuration</th>
			            		<td>
												<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_0" value="" onkeypress="" onkeyup="" onBlur=""/>&nbsp;:
												<input type="text" maxlength="1" class="input_3_table" name="wans_lb_ratio_1" value="" onkeypress="" onkeyup="" onBlur=""/>
											</td>
			          		</tr>

									</table>

        			
        	<!-- manually assigned the DHCP List end-->		
					<div class="apply_gen">
						<input class="button_gen" onclick="applyRule()" type="button" value="<#CTL_apply#>"/>
					</div>		
      	  </td>
		</tr>

							</tbody>
						</table>
					</td>
				</tr>
			</table>
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>
</form>
<div id="footer"></div>
</body>
</html>
