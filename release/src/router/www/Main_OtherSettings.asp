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
<title>ASUS Wireless Router <#Web_Title#> - WakeOnLAN</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">

<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script type="text/javascript" language="JavaScript" src="/detect.js"></script>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function initial()
{
	show_menu();
	set_rstats_location();
	hide_rstats_storage(document.form.rstats_location.value);
}

function set_rstats_location()
{
	rstats_loc = '<% nvram_get("rstats_path"); %>';

	if (rstats_loc === "")
	{
		document.form.rstats_location.value = "0";
	}
	else if (rstats_loc === "nvram")
	{
		document.form.rstats_location.value = "2";
	} 
	else 
	{ 
		document.form.rstats_location.value = "1";
	}

}

function hide_rstats_storage(_value){

        $("rstats_new_tr").style.display = (_value == "1" || _value == "2") ? "" : "none";
        $("rstats_stime_tr").style.display = (_value == "1" || _value == "2") ? "" : "none";
        $("rstats_path_tr").style.display = (_value == "1") ? "" : "none";
}

function applyRule(){

	if (document.form.rstats_location.value = 2)
	{
		document.form.rstats_path = "*nvram";
	} else if (document.form.rstats_location.value = 0)
	{
		document.form.rstats_path = "";
	}


	showLoading();
	document.form.submit();
}


function done_validating(action){
        refreshpage();    
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Main_OtherSettings.asp">
<input type="hidden" name="next_page" value="Main_OtherSettings.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_rstats">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div></td>
    <td valign="top">
        <div id="tabMenu" class="submenuBlock"></div>

      <!--===================================Beginning of Main Content===========================================-->
      <table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        <tr>
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
                <div>&nbsp;</div>
                <div class="formfonttitle">Bandwidth Monitoring</div>
                <div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
		<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">

			<tr>
	                <th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(0, 5);">Traffic history location</a></th>
   		        	<td>
                			<select name="rstats_location" class="input_option" onchange="hide_rstats_storage(this.value);">
                                	<option value="0">RAM (Default)</option>
                                	<option value="1">Custom location</option>
                	        	<option value="2">NVRam</option>
					</select>
        			</td>
			</tr>

			<tr id="rstats_stime_tr">
			<th>Save frequency:</th>
				<td>
					<select name="rstats_stime" class="input_option" >
					<option value="1" <% nvram_match("rstats_stime", "1","selected"); %>>Every 1 hour</option>
               				<option value="6" <% nvram_match("rstats_stime", "6","selected"); %>>Every 6 hours</option>
               				<option value="12" <% nvram_match("rstats_stime", "12","selected"); %>>Every 12 hours</option>
               				<option value="24" <% nvram_match("rstats_stime", "24","selected"); %>>Every 1 day</option>
               				<option value="72" <% nvram_match("rstats_stime", "72","selected"); %>>Every 3 days</option>
               				<option value="168" <% nvram_match("rstats_stime", "168","selected"); %>>Every 1 week</option>
					</select>
				</td>
			</tr>
			<tr id="rstats_path_tr">
			<th>Save history location:</th>
               	                <td><input type="text" id="rstats_path" size=32 maxlength=90 name="rstats_path" class="input_32_table" value="<% nvram_get("rstats_path"); %>"></td>
			</tr>
			<tr id="rstats_new_tr">
        		<th>Create or reset data files:<br><i>Enable if using a new location</i></th>
	        	<td>
               			<input type="radio" name="rstats_new" class="input" value="1" <% nvram_match_x("", "rstats_new", "1", "checked"); %>><#checkbox_Yes#>
		                <input type="radio" name="rstats_new" class="input" value="0" <% nvram_match_x("", "rstats_new", "0", "checked"); %>><#checkbox_No#>
       	        	</td>      	
        		</tr>
			<tr>
		        <th>Starting day of monthly cycle</th>
		        <td>
              			<input type="text" maxlength="2" class="input_3_table" name="rstats_offset" onKeyPress="return is_number(this,event);" onblur="validate_number_range(this, 1, 31)" value="<% nvram_get("rstats_offset"); %>">
		        </td>
		        </tr>
		</table>


                <div class="apply_gen">
                <input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
	        </div>
		</td></tr>
	        </tbody>
            </table>
            </form>
            </td>

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

