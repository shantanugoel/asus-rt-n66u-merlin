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
<title>ASUS Wireless Router <#Web_Title#> - <#menu5_3_6#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" language="JavaScript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script type="text/javaScript" src="/jquery.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% wanlink(); %>

<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
var $j = jQuery.noConflict();

function init(){
		show_banner(1);
		show_footer();
		ddns_load_body();	
}

function valid_wan_ip() {
        // test if WAN IP is a private IP.
        var A_class_start = inet_network("10.0.0.0");
        var A_class_end = inet_network("10.255.255.255");
        var B_class_start = inet_network("172.16.0.0");
        var B_class_end = inet_network("172.31.255.255");
        var C_class_start = inet_network("192.168.0.0");
        var C_class_end = inet_network("192.168.255.255");
        
        var ip_obj = wanlink_ipaddr();
        var ip_num = inet_network(ip_obj);
        var ip_class = "";

        if(ip_num > A_class_start && ip_num < A_class_end)
                ip_class = 'A';
        else if(ip_num > B_class_start && ip_num < B_class_end)
                ip_class = 'B';
        else if(ip_num > C_class_start && ip_num < C_class_end)
                ip_class = 'C';
        else if(ip_num != 0){
		showhide("wan_ip_hide2", 0);
		showhide("wan_ip_hide3", 1);
		return;
        }
	showhide("wan_ip_hide2", 1);
	showhide("wan_ip_hide3", 0);
	return;
}

function ddns_load_body(){
	show_menu();
	valid_wan_ip();

	var hostname_x = '<% nvram_get("ddns_hostname_x"); %>';
	var ddns_return_code = '<% nvram_get_ddns("LANHostConfig","ddns_return_code"); %>';
	var ddns_old_name = '<% nvram_get("ddns_old_name"); %>';
	var ddns_server_x = '<% nvram_get("ddns_server_x"); %>';
	
	if(document.form.ddns_server_x.selectedIndex == 0 && hostname_x != ''){
		ddns_hostname_title = hostname_x.substring(0, hostname_x.indexOf('.'));
		showhide("ddnsname_input", 0);
		showhide("asusddnsname_input", 1);
		$("DDNSName").value = ddns_hostname_title;
	}
	else if(document.form.ddns_server_x.selectedIndex == 0 && hostname_x == ''){
		showhide("ddnsname_input", 0);
		showhide("asusddnsname_input", 1);		
		$("DDNSName").value = "<#asusddns_inputhint#>";
	}else if(document.form.ddns_server_x.selectedIndex != 0 && hostname_x == ''){
		showhide("ddnsname_input", 1);
		showhide("asusddnsname_input", 0);
		$("ddns_hostname_x").value = "<#asusddns_inputhint#>";
	}else{
		showhide("ddnsname_input", 1);
		showhide("asusddnsname_input", 0);
		$("ddns_hostname_x").value = hostname_x;		
	}	
	
	hideLoading();

	if(ddns_return_code == 'register,-1')
		alert("<#LANHostConfig_x_DDNS_alarm_2#>");
	else if(ddns_return_code == 'register,200'){
		alert("<#LANHostConfig_x_DDNS_alarm_3#>");
		showhide("wan_ip_hide2", 0);
		showhide("wan_ip_hide3", 1);		
	}else if(ddns_return_code == 'register,203')
		alert("<#LANHostConfig_x_DDNS_alarm_hostname#> '"+hostname_x+"' <#LANHostConfig_x_DDNS_alarm_registered#>");
	else if(ddns_return_code == 'register,220'){
		alert("<#LANHostConfig_x_DDNS_alarm_4#>");
		showhide("wan_ip_hide2", 0);
		showhide("wan_ip_hide3", 1);		
	}else if(ddns_return_code == 'register,230'){
		alert("<#LANHostConfig_x_DDNS_alarm_5#>");
		showhide("wan_ip_hide2", 0);
		showhide("wan_ip_hide3", 1);		
	}else if(ddns_return_code == 'register,233')
		alert("<#LANHostConfig_x_DDNS_alarm_hostname#> '"+hostname_x+"' <#LANHostConfig_x_DDNS_alarm_registered_2#> '"+ddns_old_name+"'.");
	else if(ddns_return_code == 'register,296')
		alert("<#LANHostConfig_x_DDNS_alarm_6#>");
	else if(ddns_return_code == 'register,297')
		alert("<#LANHostConfig_x_DDNS_alarm_7#>");
	else if(ddns_return_code == 'register,298')
		alert("<#LANHostConfig_x_DDNS_alarm_8#>");
	else if(ddns_return_code == 'register,299')
		alert("<#LANHostConfig_x_DDNS_alarm_9#>");
	else if(ddns_return_code == 'register,401')
		alert("<#LANHostConfig_x_DDNS_alarm_10#>");
	else if(ddns_return_code == 'register,407')
		alert("<#LANHostConfig_x_DDNS_alarm_11#>");
	else if(ddns_return_code == 'time_out')
		alert("<#LANHostConfig_x_DDNS_alarm_1#>");
	else if(ddns_return_code =='unknown_error')
		alert("<#LANHostConfig_x_DDNS_alarm_2#>");
        else if(ddns_return_code =='connect_fail')
		alert("<#qis_fail_desc7#>");
	
	if(ddns_server_x == "WWW.ASUS.COM" || ddns_server_x == ""){
		inputCtrl(document.form.ddns_username_x, 0);
		inputCtrl(document.form.ddns_passwd_x, 0);
		document.form.ddns_wildcard_x[0].disabled= 1;
		document.form.ddns_wildcard_x[1].disabled= 1;
		showhide("link", 0);
		//showhide("Hostname_Note", 1);
	}
	else{
		inputCtrl(document.form.ddns_username_x, 1);
		inputCtrl(document.form.ddns_passwd_x, 1);
		document.form.ddns_wildcard_x[0].disabled = 0;
		document.form.ddns_wildcard_x[1].disabled = 0;
		//showhide("Hostname_Note", 0);
	}
}

function applyRule(){
	if(validForm()){
		with(document.form){
			//Viz 2011.09
			//if(ddns_enable_x[0].checked && ddns_server_x.value == 'WWW.ASUS.COM' && ddns_hostname_x.value.length > '.asuscomm.com'.length)/*&& !confirm('<#ddnsname_computername#>'))*/
				//return;
		}
		
		showLoading();
		document.form.submit();	
	}
}

function validForm(){
	if(document.form.ddns_server_x.value == "WWW.ASUS.COM"){
		if(!validate_string(document.form.DDNSName)
				|| !validate_ddns_hostname(document.form.DDNSName)){
			document.form.DDNSName.focus();
			document.form.DDNSName.select();				
			return false;
	}
	}else{
		if(!validate_string(document.form.ddns_username_x)
				|| !validate_string(document.form.ddns_passwd_x))
			return false;
	}
	
		
	if(document.form.ddns_enable_x[0].checked){
		
		if(document.form.ddns_server_x.selectedIndex == 0){		//WWW.ASUS.COM	
			if(document.form.DDNSName.value == ""){
					alert("<#LANHostConfig_x_DDNS_alarm_14#>");
					document.form.DDNSName.focus();
					document.form.DDNSName.select();
					return false;
			}else{
					document.form.ddns_hostname_x.value = document.form.DDNSName.value+".asuscomm.com";		
					return true;		
			}
		}else{		
		
		if(document.form.ddns_hostname_x.value == ""){
			alert("<#LANHostConfig_x_DDNS_alarm_14#>");
			document.form.ddns_hostname_x.focus();
			document.form.ddns_hostname_x.select();
			return false;
		}
		else if(document.form.ddns_server_x.selectedIndex != 0 && document.form.ddns_username_x.value == ""){
			alert("<#QKSet_account_nameblank#>");
			document.form.ddns_username_x.focus();
			document.form.ddns_username_x.select();
			return false;
		}
		else if(document.form.ddns_server_x.selectedIndex != 0 && document.form.ddns_passwd_x.value == ""){
			alert("<#File_Pop_content_alert_desc6#>");
			document.form.ddns_passwd_x.focus();
			document.form.ddns_passwd_x.select();
			return false;
		}
		else
			return true;
	}
	}
	else
		return true;
}

function checkDDNSReturnCode(){
    $j.ajax({
    	url: '/ajax_ddnscode.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		checkDDNSReturnCode();
    	},
    	success: function(response){
                if(ddns_return_code == 'ddns_query')
        	        setTimeout("checkDDNSReturnCode();", 500);
                else 
                        refreshpage();
       }
   });
}

</script>
</head>

<body onload="init();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="wan_route_x" value="<% nvram_get("wan_route_x"); %>">
<input type="hidden" name="wan_nat_x" value="<% nvram_get("wan_nat_x"); %>">

<input type="hidden" name="current_page" value="Advanced_ASUSDDNS_Content.asp">
<input type="hidden" name="next_page" value="">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="group_id" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_ddns">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
	<tr>
		<td width="17">&nbsp;</td>
		
		<td valign="top" width="202">				
		<div  id="mainMenu"></div>	
		<div  id="subMenu"></div>		
		</td>				
		
    <td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
		<!--===================================Beginning of Main Content===========================================-->
<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
	<tr>
		<td align="left" valign="top" >
          
		<table width="760px" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
		<tbody>
			<tr>
		  		<td bgcolor="#4D595D" valign="top"  >
		  		<div>&nbsp;</div>
		  		<div class="formfonttitle"><#menu5_3#> - <#menu5_3_6#></div>
		  		<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		 		  <div class="formfontdesc"><#LANHostConfig_x_DDNSEnable_sectiondesc#></div>
				  <div class="formfontdesc" id="wan_ip_hide2" style="color:#FFCC00;"><#LANHostConfig_x_DDNSEnable_sectiondesc2#></div>
					<div class="formfontdesc" id="wan_ip_hide3" style="color:#FFCC00;"><#LANHostConfig_x_DDNSEnable_sectiondesc3#></div>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<input type="hidden" name="wl_gmode_protection_x" value="<% nvram_get("wl_gmode_protection_x"); %>">
			<tr>
				<th><#LANHostConfig_x_DDNSEnable_itemname#></th>
				<td>
				<input type="radio" value="1" name="ddns_enable_x"onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_enable_x', '1')" <% nvram_match("ddns_enable_x", "1", "checked"); %>><#checkbox_Yes#>
				<input type="radio" value="0" name="ddns_enable_x"onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_enable_x', '0')" <% nvram_match("ddns_enable_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr>		
			<tr>
				<th><#LANHostConfig_x_DDNSServer_itemname#></th>
				<td>
                  		<select name="ddns_server_x"class="input_option"onchange="return change_common(this, 'LANHostConfig', 'ddns_server_x')">
                    			<option value="WWW.ASUS.COM" <% nvram_match("ddns_server_x", "WWW.ASUS.COM","selected"); %>>WWW.ASUS.COM</option>
                    			<option value="WWW.DYNDNS.ORG" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG","selected"); %>>WWW.DYNDNS.ORG</option>
                    			<option value="WWW.DYNDNS.ORG(CUSTOM)" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG(CUSTOM)","selected"); %>>WWW.DYNDNS.ORG(CUSTOM)</option>
                    			<option value="WWW.DYNDNS.ORG(STATIC)" <% nvram_match("ddns_server_x", "WWW.DYNDNS.ORG(STATIC)","selected"); %>>WWW.DYNDNS.ORG(STATIC)</option>
                    			<option value="WWW.TZO.COM" <% nvram_match("ddns_server_x", "WWW.TZO.COM","selected"); %>>WWW.TZO.COM</option>
                    			<option value="WWW.ZONEEDIT.COM" <% nvram_match("ddns_server_x", "WWW.ZONEEDIT.COM","selected"); %>>WWW.ZONEEDIT.COM</option>
                    			<option value="WWW.DNSOMATIC.COM" <% nvram_match("ddns_server_x", "WWW.DNSOMATIC.COM","selected"); %>>WWW.DNSOMATIC.COM</option>
					<option value="WWW.NO-IP.COM" <% nvram_match("ddns_server_x",
"WWW.NO-IP.COM","selected"); %>>WWW.NO-IP.COM</option>
                  		</select>
				<a id="link" href="javascript:openLink('x_DDNSServer')" style=" margin-left:5px; text-decoration: underline;"><#LANHostConfig_x_DDNSServer_linkname#></a>
				</td>
			</tr>
			<tr>
				<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(5,13);"><#LANHostConfig_x_DDNSHostNames_itemname#></a></th>
				<td>
					<div id="ddnsname_input" style="display:none;">
						<input type="text" maxlength="63" class="input_25_table" name="ddns_hostname_x" id="ddns_hostname_x" value="<% nvram_get("ddns_hostname_x"); %>" onKeyPress="return is_string(this, event)">
					</div>
					<div id="asusddnsname_input" style="display:none;">
						<input type="text" maxlength="20" class="input_15_table" name="DDNSName" id="DDNSName" class="inputtext">.asuscomm.com
					</div>							
							
				</td>
			</tr>			
			<tr>
				<th><#LANHostConfig_x_DDNSUserName_itemname#></th>
				<td><input type="text" maxlength="32" class="input_25_table" name="ddns_username_x" value="<% nvram_get("ddns_username_x"); %>" onKeyPress="return is_string(this, event)"></td>
			</tr>
			<tr>
				<th><#LANHostConfig_x_DDNSPassword_itemname#></th>
				<td><input type="password" maxlength="64" class="input_25_table" name="ddns_passwd_x" value="<% nvram_get("ddns_passwd_x"); %>"></td>
			</tr>
			<tr>
				<th><#LANHostConfig_x_DDNSWildcard_itemname#></th>
				<td>
					<input type="radio" value="1" name="ddns_wildcard_x" onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_wildcard_x', '1')" <% nvram_match("ddns_wildcard_x", "1", "checked"); %>><#checkbox_Yes#>
					<input type="radio" value="0" name="ddns_wildcard_x" onClick="return change_common_radio(this, 'LANHostConfig', 'ddns_wildcard_x', '0')" <% nvram_match("ddns_wildcard_x", "0", "checked"); %>><#checkbox_No#>
				</td>
			</tr>
			<tr>
				<th><#LANHostConfig_x_DDNSStatus_itemname#></th>
				<td>
					<input type="hidden" maxlength="15" class="button_gen" size="12" name="" value="<% nvram_get("DDNSStatus"); %>">
				  	<input type="submit" maxlength="15" class="button_gen" onclick="showLoading();return onSubmitApply('ddnsclient');" size="12" name="LANHostConfig_x_DDNSStatus_button" value="<#LANHostConfig_x_DDNSStatus_buttonname#>" /></td>
			</tr>
		</table>
				<div class="apply_gen">
					<input class="button_gen" onclick="applyRule();" type="button" value="<#CTL_apply#>" />
				</div>		
		
			  </td>
              </tr>
            </tbody>

            </table>
	  </td>
</form>
        </tr>
      </table>				
		<!--===================================Ending of Main Content===========================================-->
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>
