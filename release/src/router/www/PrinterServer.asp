<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=EmulateIE7"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title>ASUS Wireless Router <#Web_Title#> - <#Network_Printer_Server#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link href="/sliderplugin/slider_css.css" rel="stylesheet" type="text/css" /> 
<style type="text/css"> 
	div.wrapper { margin: 0 auto; width: 730px;}
	td.sidenav { width:200px;}
	body {font-family: Verdana, Tohoma, Arial, Helvetica, sans-serif;padding:0;margin:0;}
	.wrapperDesc { margin: 0 auto; width: 570px;}
</style> 
<link rel="stylesheet" href="/sliderplugin/jquery.tabs.css" type="text/css" media="print, projection, screen"> 
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script src="/sliderplugin/jquery-easing.1.2.pack.js" type="text/javascript"></script> 
<script src="/sliderplugin/jquery-easing-compatibility.1.2.pack.js" type="text/javascript"></script> 
<script src="/sliderplugin/coda-slider.1.1.1.pack.js" type="text/javascript"></script> 
<script>
var $j = jQuery.noConflict();
jQuery(window).bind("load", function() {
	jQuery("div#slider1").codaSlider();
	jQuery("div#slider2").codaSlider();
});
</script>
<style type="text/css">
.printerServer_table{
	width:740px;
	padding:5px; 
	padding-top:20px; 
	margin-top:-16px; 
	position:relative;
	background-color:#4d595d;
	align:left;
	-webkit-border-radius: 3px;
	-moz-border-radius: 3px;
	border-radius: 3px;
	height: 800px;
}
.line_export{
	height:20px;
	width:736px;
}
.desctitle{
	font-size: 14px;
	font-weight: bolder;
	margin-left: 20px;
	margin-top: 15px;
}
.desc{
	margin-left: 20px;
	margin-top: 10px;
}
.descimage{
	margin-left: 20px;
	margin-top: 5px;
}
.statusBar{
	margin:auto;
}
.MethodDesc{
	font-style: italic;
	color: #999;
}
.imdShade{
	-moz-box-shadow: 15px 15px 10px rgba(0, 0, 0, 0.6);
	-webkit-box-shadow: 15px 15px 10px rgba(0, 0, 0, 0.6);
	box-shadow: 15px 15px 10px #333;
}
</style>
<script>
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

function initial(){
	show_menu();
	$("option5").innerHTML = '<table><tbody><tr><td><img border="0" width="50px" height="50px" src="images/New_ui/icon_index_5.png" style="margin-top:-3px;"></td><td><div style="width:120px;"><#Menu_usb_application#></div></td></tr></tbody></table>';
	$("option5").className = "m5_r";
	setTimeout("showMethod('','none');", 100);
}

function showMethod(flag1, flag2){
	document.getElementById("method1").style.display = flag1;
	document.getElementById("method1Title").style.display = flag1;
	document.getElementById("method2").style.display = flag2;
	document.getElementById("method2Title").style.display = flag2;
	if(flag1 == ""){
		$("help1").style.color = "#FFF";
		$("help2").style.color = "gray";
	}
	else{
		$("help1").style.color = "gray";
		$("help2").style.color = "#FFF";
	}
}
</script>
</head>

<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>


<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>

<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
</form>

<table class="content" align="center" cellspacing="0" style="margin:auto;">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
	<td valign="top" width="202">
	  <div id="mainMenu"></div>
	  <div id="subMenu"></div>
	</td>
	
  <td valign="top">
		<div id="tabMenu" class="submenuBlock"></div>
		<br>

<!--=====Beginning of Main Content=====-->
<div class="printerServer_table">
<table>
  <tr>
  	<td class="formfonttitle"><#Network_Printer_Server#>
			<img onclick="go_setting('/APP_Installation.asp')" align="right" style="cursor:pointer;margin-right:10px;margin-top:-10px" title="Back to USB Extension" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'">
		</td>
  </tr> 
  <tr>
  	<td class="line_export"><img src="images/New_ui/export/line_export.png" /></td>
  </tr>
  <tr>
   	<td><div class="formfontdesc"><#Network_Printer_desc#></div></td> 
  </tr>
  <tr>
   	<td>
		<div id="mainbody">
			<div class="wrapper">
				<div class="shadow-l">
					<div class="shadow-r">
						<table class="" cellspacing="0" cellpadding="0">
							<tbody><tr valign="top">
								<td class="">
									<div class="padding">
										<div class="">
											<ul class="">
												<li>
														<span id="help1" onclick="showMethod('','none');" style="cursor:pointer;text-decoration:underline;font-weight:bolder;"><#asus_ez_print_share#></span>&nbsp;&nbsp;
														<a href="http://dlcdnet.asus.com/pub/ASUS/wireless/ASUSWRT/Printer.zip" style="text-decoration:underline;font-size:14px;font-weight:bolder;color:#FC0"">Download Now!</a>
												</li>
												<li><p id="help2" onclick="showMethod('none','');" style="cursor:pointer;text-decoration:underline;font-weight:bolder;"><#LPR_print_share#></p></li>
											</ul>	
											<br/>
											<div class="moduletable">
												<center>		
													<div id="container">
														<div class="top-heading" id="method1Title">
															<a name="Windows_Mapped_Shares"></a>
													  	<!--h2><#asus_ez_print_share#></h2-->
													  	<div style="font-style:italic;margin-left:75px;width:550px;"><#asus_ezps_note#></div>
														</div>
														<div class="slider-wrap" id="method1">
															<div class="stripNavL" id="stripNavL0" title="<#btn_pre#>"><a href="#">Left</a></div><div class="stripNav" id="stripNav0" style="width: 10px; "><ul><li class="tab1" style=""><a href="#1">Step 1</a></li><li class="tab2" style=""><a href="#2">Step 2</a></li><li class="tab3" style=""><a href="#3">Step 3</a></li><li class="tab4" style=""><a href="#4" class="current">Step 4</a></li><li class="tab5" style=""><a href="#5">Step 5</a></li></ul></div><div id="slider1" class="stripViewer">
																<div class="panelContainer" style="width: 3000px; left: -1800px; ">																		
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD;">Step 1 <br/></b></font><#asus_ezps_step1#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/USBPrinter1.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>
																	<div class="panel">	
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 2 <br/></b></font><#asus_ezps_step2#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/USBPrinter2.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>		
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 3 <br/></b></font><#asus_ezps_step3#>
																				<!-- Wait for a few minutes for the initial setup to finish. Click Next -->
																			</div>
																			<p>
																				<img src="/images/PrinterServer/USBPrinter3.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 4 <br/></b></font><#asus_ezps_step4#>
																				 <!-- Key in the IP address of the wireless router in the Printer Name of IP Address field and click Next. -->
																			</div>
																			<p>
																				<img src="/images/PrinterServer/USBPrinter4.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>

																	<!--div class="panel">	
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 5 <br/></b></font>
																				Follow the Windows OS instruction to install the Printer driver.
																			</div>
																			<p>
																				<img src="http://192.168.123.98/v3_menu/images2/qweb/mapped_drive-005.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div-->
																				
																</div>
															</div><div class="stripNavR" id="stripNavR0" title="<#btn_next#>"><a href="#">Right</a></div>
														</div>		
												
														<div class="top-heading" id="method2Title">
															<a name="FTP_Upload"></a>
													  	<!--h2><#LPR_print_share#></h2-->
													  	<div style="font-style: italic;margin-left:75px;width:550px;"><#LPR_ps_note#></div>	
														</div>														
														<div class="slider-wrap" id="method2">
															<div class="stripNavL" id="stripNavL1" title="<#btn_pre#>"><a href="#">Left</a></div><div class="stripNav" id="stripNav1" style="width: 6px; "><ul><li class="tab1" style=""><a href="#1" class="current">Step 1</a></li><li class="tab2" style=""><a href="#2">Step 2</a></li><li class="tab3" style=""><a href="#3">Step 3</a></li></ul></div><div id="slider2" class="stripViewer">
																<div class="panelContainer" style="width: 3000px; left: -1800px; ">																		
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 1<br/></b></font><#LPR_ps_step1#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR1.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>
																	<div class="panel">
																		<div class="wrapperDesc">																			
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 2<br/></b></font><#LPR_ps_step2#>																				
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR2.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>		
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 3<br/></b></font><#LPR_ps_step3#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR3.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 4<br/></b></font><#LPR_ps_step4#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR4.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 5<br/></b></font><#LPR_ps_step5#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR5.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 6<br/></b></font><#LPR_ps_step6#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR6.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 7<br/></b></font><#LPR_ps_step7#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR7.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 8<br/></b></font><#LPR_ps_step8#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR8.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 9<br/></b></font><#LPR_ps_step9#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR9.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																	<div class="panel">
																		<div class="wrapperDesc">
																			<div style="color: #FFF; text-align: left;">
																				<font size="3"><b style="color:#5AD">Step 10<br/></b></font><#LPR_ps_step10#>
																			</div>
																			<p>
																				<img src="/images/PrinterServer/LPR10.jpg" border="0" hspace="0" vspace="0" alt="" class="imdShade">
																			</p>
																		</div>
																	</div>																
																</div>
															</div><div class="stripNavR" id="stripNavR1" title="<#btn_next#>"><a href="#">Right</a></div>
														</div>		
							      
													</div>														
												</center>			
											</div>
										</div>	
									<span class="article_seperator">&nbsp;</span>
								</div>
							</td>
						</tr>
					</tbody></table>													
				</div>
			</div>
		</div>
		</div>

		</td>
  </tr>
  </table>

<!--=====End of Main Content=====-->
		</td>

		<td width="20" align="center" valign="top"></td>
	</tr>
</table>

<div id="footer"></div>
</body>
</html>

