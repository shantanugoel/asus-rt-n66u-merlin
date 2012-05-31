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
<title><#Web_Title#> - <#menu3#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="/ext/css/ext-all.css">
<style type="text/css">
/* folder tree */
.mask_bg{
	position:absolute;	
	margin:auto;
	top:0;
	left:0;
	width:100%;
	height:100%;
	z-index:100;
	background:url(/images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	overflow:hidden;
}
.mask_floder_bg{
	position:absolute;	
	margin:auto;
	top:0;
	left:0;
	width:100%;
	height:100%;
	z-index:300;
	background:url(/images/popup_bg2.gif);
	background-repeat: repeat;
	filter:progid:DXImageTransform.Microsoft.Alpha(opacity=60);
	-moz-opacity: 0.6;
	display:none;
	overflow:hidden;
}
.panel{
	width:450px;
	position:absolute;
	margin-top:-8%;
	margin-left:35%;
	z-index:200;
	display:none;
}
.floder_panel{
	background-color:#999;	
	border:2px outset #CCC;
	font-size:15px;
	font-family:Verdana, Geneva, sans-serif;
	color:#333333;
	width:450px;
	position:absolute;
	margin-top:-8%;
	margin-left:35%;
	z-index:400;
	display:none;
}
</style>
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/detect.js"></script>
<script type="text/javascript" src="/jquery.js"></script>
<script>
var $j = jQuery.noConflict();
<% login_state_hook(); %>
var wireless = [<% wl_auth_list(); %>];	// [[MAC, associated, authorized], ...]
wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';

var cloud_sync;
var cloud_status = "";
var cloud_obj = "";
var cloud_msg = "";

var cloud_synclist_array = decodeURIComponent('<% nvram_char_to_ascii("", "cloud_sync"); %>').replace(/>/g, "&#62").replace(/</g, "&#60"); // type>user>password>url>dir>rule>enable
var isEdit = 0;
var isonEdit = 0;
var maxrulenum = 1;
var rulenum = 1;

function initial(){
	show_menu();
	showAddTable();
	showcloud_synclist();

	if(rulenum < maxrulenum){
		setTimeout('addNewScript("/ext/ext-base.js");', 500);
		setTimeout('addNewScript("/ext/ext-all.js");', 500);
	}
}

function addRow(obj, head){
	if(head == 1)
		cloud_synclist_array += "&#62";
	else
		cloud_synclist_array += "&#60";
			
	cloud_synclist_array += obj.value;
	obj.value = "";
}

function addRow_Group(upper){ 
	var rule_num = $('cloud_synclist_table').rows.length;
	var item_num = $('cloud_synclist_table').rows[0].cells.length;
	
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;	
	}			
				
	Do_addRow_Group();		
}

function Do_addRow_Group(){		
	addRow(document.form.cloud_sync_type ,1);
	addRow(document.form.cloud_sync_username, 0);
	addRow(document.form.cloud_sync_password, 0);
	addRow(document.form.cloud_sync_dir, 0);
	addRow(document.form.cloud_sync_rule, 0);
	showcloud_synclist();
}

function edit_Row(r){ 	
	var i=r.parentNode.parentNode.rowIndex;
  	
	document.form.cloud_sync_type.value = $('cloud_synclist_table').rows[i].cells[0].innerHTML;
	document.form.cloud_sync_username.value = $('cloud_synclist_table').rows[i].cells[1].innerHTML; 
	document.form.cloud_sync_password.value = $('cloud_synclist_table').rows[i].cells[2].innerHTML; 
	document.form.cloud_sync_dir.value = $('cloud_synclist_table').rows[i].cells[3].innerHTML;
	document.form.cloud_sync_rule.value = $('cloud_synclist_table').rows[i].cells[4].innerHTML;

  del_Row(r);	
}

function del_Row(r){
	if(isonEdit == 1)
		return false;

	var cloud_synclist_row = cloud_synclist_array.split('&#60'); // resample
  var i=r.parentNode.parentNode.rowIndex+1;

  var cloud_synclist_value = "";
	for(k=1; k<cloud_synclist_row.length; k++){
		if(k == i)
			continue;
		else
			cloud_synclist_value += "&#60";

		var cloud_synclist_col = cloud_synclist_row[k].split('&#62');
		for(j=0; j<cloud_synclist_col.length-1; j++){
			cloud_synclist_value += cloud_synclist_col[j];
			if(j != cloud_synclist_col.length-1)
				cloud_synclist_value += "&#62";
		}
	}

	isonEdit = 1;
	cloud_synclist_array = cloud_synclist_value;
	showcloud_synclist();
	document.form.cloud_sync.value = cloud_synclist_array.replace(/&#62/g, ">").replace(/&#60/g, "<");
	$("update_scan").style.display = '';
	FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
	showLoading();
	document.form.submit();
}

function showcloud_synclist(){
	if(cloud_synclist_array != ""){
		$("creatBtn").style.display = "none";
	}

	rulenum = 0;
	var cloud_synclist_row = cloud_synclist_array.split('&#60');
	var code = "";

	code +='<table width="99%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="cloud_synclist_table">';
	if(cloud_synclist_array == "")
		code +='<tr height="55px"><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td>';
	else{
		for(var i = 0; i < cloud_synclist_row.length; i++){
			rulenum++;
			code +='<tr id="row'+i+'" height="55px">';
			var cloud_synclist_col = cloud_synclist_row[i].split('&#62');
			var wid = [10, 25, 0, 0, 10, 30, 15];
			for(var j = 0; j < cloud_synclist_col.length; j++){
				if(j == 2 || j == 3){
					continue;
				}
				else{
					if(j == 0)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/ASUS-WebStorage.png"></td>';
					else if(j == 1)
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><span style="font-size:16px;font-family: Calibri;font-weight: bolder;">'+ cloud_synclist_col[j] +'</span></td>';
					else if(j == 4){
						if(cloud_synclist_col[j] == 2)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/left.gif"></td>';
						else if(cloud_synclist_col[j] == 1)
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/Right.gif"></td>';
						else
							code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span><img width="30px" src="/images/cloudsync/left_right.gif"></td>';
					}
					else if(j == 6){
						code +='<td width="'+wid[j]+'%" id="cloudStatus"></td>';
					}
					else
						code +='<td width="'+wid[j]+'%"><span style="display:none">'+ cloud_synclist_col[j] +'</span>'+ cloud_synclist_col[j] +'</td>';
				}
			}
			code +='<td width="10%"><input class="remove_btn" onclick="del_Row(this);" value=""/></td>';
		}
		updateCloudStatus();
	}
  code +='</table>';
	$("cloud_synclist_Block").innerHTML = code;
}

function updateCloudStatus(){
    $j.ajax({
    	url: '/update_cloudstatus.asp',
    	dataType: 'script', 

    	error: function(xhr){
      		updateCloudStatus();
    	},
    	success: function(response){
					// handle msg
					var _cloud_msg;
					if(cloud_obj != ""){
						_cloud_msg =  "<p style='font-weight: bolder;'>";
						_cloud_msg += cloud_status;
						_cloud_msg += ": </p>";
						_cloud_msg += decodeURIComponent(cloud_obj);
					}
					else if(cloud_msg)
						_cloud_msg = cloud_msg;
					else
						_cloud_msg = "No log.";
	
					// handle status
					var _cloud_status;
					if(cloud_status != "")
						_cloud_status = cloud_status + "..";
					else
						_cloud_status = "";

					if($("cloudStatus"))
						$("cloudStatus").innerHTML = '<a style="text-decoration:underline; cursor:pointer" onmouseout="return nd();" onclick="return overlib(\''+ _cloud_msg +'\');">'+ _cloud_status +'</a>';
			 		setTimeout("updateCloudStatus();", 2000);
      }
   });
}

function validform(){
	if(document.form.cloud_username.value == '' ||	document.form.cloud_password.value == '' ||	document.form.cloud_dir.value == ''){
		return false;
	}

	return true;
}

function applyRule(){
	if(validform()){
		isonEdit = 1;
		cloud_synclist_array += '0&#62'+document.form.cloud_username.value+'&#62'+document.form.cloud_password.value+'&#62none&#62'+document.form.cloud_rule.value+'&#62'+document.form.cloud_dir.value+'&#621';
		showcloud_synclist();
	
		document.form.cloud_sync.value = cloud_synclist_array.replace(/&#62/g, ">").replace(/&#60/g, "<");
		document.form.cloud_username.value = '';
		document.form.cloud_password.value = '';
		document.form.cloud_rule.value = '';
		document.form.cloud_dir.value = '';
		isEdit = 0;
		showAddTable();
		$("update_scan").style.display = '';
		FormActions("start_apply.htm", "apply", "restart_cloudsync", "3");
		showLoading();
		document.form.submit();
	}
}

function showAddTable(){
	if(isEdit == 1){ // edit
		if(typeof Ext == 'undefined'){
			addNewScript("/ext/ext-base.js");
			addNewScript("/ext/ext-all.js");
		}

		$j("#cloudAddTable").fadeIn();
		$("creatBtn").style.display = "none";
		$("applyBtn").style.display = "";
	}
	else{ // list
		$("cloudAddTable").style.display = "none";
		$("creatBtn").style.display = "";
		$("applyBtn").style.display = "none";
	}
}

// get folder tree
var dm_dir = new Array(); 
var Download_path = 'Download2';
var WH_INT=0,Floder_WH_INT=0,General_WH_INT=0;
var BASE_PATH;
var folderlist = new Array();

function showPanel(){
	WH_INT = setInterval("getWH();",1000);
 	$j("#DM_mask").fadeIn(1000);
  $j("#panel_add").show(1000);
	create_tree();
}

function getWH(){
	var winWidth;
	var winHeight;
	winWidth = document.documentElement.scrollWidth;
	if(document.documentElement.clientHeight > document.documentElement.scrollHeight)
		winHeight = document.documentElement.clientHeight;
	else
		winHeight = document.documentElement.scrollHeight;
	$("DM_mask").style.width = winWidth+"px";
	$("DM_mask").style.height = winHeight+"px";
}

function getFloderWH(){
	var winWidth;
	var winHeight;
	winWidth = document.documentElement.scrollWidth;

	if(document.documentElement.clientHeight > document.documentElement.scrollHeight)
		winHeight = document.documentElement.clientHeight;
	else
		winHeight = document.documentElement.scrollHeight;

	$("DM_mask_floder").style.width = winWidth+"px";
	$("DM_mask_floder").style.height = winHeight+"px";
}

function show_AddFloder(){
	Floder_WH_INT = setInterval("getFloderWH();",1000);
	$j("#DM_mask_floder").fadeIn(1000);
	$j("#panel_addFloder").show(1000);
}

function hidePanel(){
	($j("#tree").children()).remove();
	clearInterval(WH_INT);
	$j("#DM_mask").fadeOut('fast');
	$j("#panel_add").hide('fast');
}

function create_tree(){
	var rootNode = new Ext.tree.TreeNode({ text:'/mnt', id:'0'});
	var rootNodechild = new Ext.tree.TreeNode({ text:'', id:'0t'});
	rootNode.appendChild(rootNodechild);
	var tree = new Ext.tree.TreePanel({
			tbar:[{text:"<#CTL_ok#>",handler:function(){$j("#PATH").attr("value",Download_path);hidePanel();}},
				'->',{text:'X',handler:function(){hidePanel();}}
			],
			title:"Please select the desire folder",
				applyTo:'tree',
				root:rootNode,
				height:400,
				autoScroll:true
	});
	tree.on('expandnode',function(node){
		var allParentNodes = getAllParentNodes(node);
		var path='';

		for(var j=0; j<allParentNodes.length; j++){
			path = allParentNodes[j].text + '/' +path;
		}

		initial_dir(path,node);
	});
	tree.on('collapsenode',function(node){
		while(node.firstChild){
			node.removeChild(node.firstChild);
		}
		var childNode = new Ext.tree.TreeNode({ text:'', id:'0t'});
		node.appendChild(childNode);
	});
	tree.on('click',function(node){
		var allParentNodes = getAllParentNodes(node);
		var path='';

		for(var j=0; j<allParentNodes.length; j++){
			if(j == allParentNodes.length-2)
				continue;
			else
				path = allParentNodes[j].text + '/' +path;
		}

		Download_path = path;
		path = BASE_PATH + '/' + path;
		var url = "getfoldertree.asp";
		var type = "General";

		url += "?motion=gettree&layer_order="+ _layer_order +"&t=" +Math.random();
		$j.get(url,function(data){initial_folderlist(data);});
	});
}

function initial_folderlist(data){
	eval("folderlist=["+data+"]");
}

function getAllParentNodes(node) {
	var parentNodes = [];
	var _nodeID = node.id;
	_layer_order = "0"

	for(i=1; i<_nodeID.length; i++)
		_layer_order += "_" + node.id[i]; // generating _layer_order for initial_dir()

	parentNodes.push(node);
	while (node.parentNode) {
		parentNodes = parentNodes.concat(node.parentNode);
		node = node.parentNode;
	}
	return parentNodes;
};

function initial_dir(path,node){
	var url = "getfoldertree.asp";
	var type = "General";

	url += "?motion=gettree&layer_order="+ _layer_order +"&t=" +Math.random();
	$j.get(url,function(data){initial_dir_status(data,node);});
}

function initial_dir_status(data,node){
	dm_dir.length = 0;
	if(data == "/" || (data != null && data != "")){
		eval("dm_dir=[" + data +"]");	
		while(node.lastChild &&(node.lastChild !=node.firstChild)) {
    			node.removeChild(node.lastChild);
		}
		for(var i=0; i<dm_dir.length; i++){
			var childNodeId = node.id +i;
			var childnode = new Ext.tree.TreeNode({id:childNodeId,text:dm_dir[i].split("#")[0]});
			node.appendChild(childnode);
			var childnodeT = new Ext.tree.TreeNode({id:childNodeId+'t',text:''});
			childnode.appendChild(childnodeT);
		}
		node.removeChild(node.firstChild);
	}
	else{
		while(node.firstChild){
			node.removeChild(node.firstChild);
		}
	}
}
</script>
</head>

<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>

<!-- floder tree-->
<div id="DM_mask" class="mask_bg"></div>
<div id="panel_add" class="panel">
	<div id="tree"></div>
</div>
<div id="DM_mask_floder" class="mask_floder_bg"></div>
<div id="panel_addFloder" class="floder_panel">
	<span style="margin-left:95px;"><b id="multiSetting_0"></b></span><br /><br />
	<span style="margin-left:8px;margin-right:8px;"><b id="multiSetting_1"></b></span>
	<input type="text" id="newFloder" class="input_15_table" value="" /><br /><br />
	<input type="button" name="AddFloder" id="multiSetting_2" value="" style="margin-left:100px;" onclick="AddFloderName();">
	&nbsp;&nbsp;
	<input type="button" name="Cancel_Floder_add" id="multiSetting_3" value="" onClick="hide_AddFloder();">
</div>
<!-- floder tree-->

<div id="Loading" class="popup_bg"></div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_sync.asp">
<input type="hidden" name="next_page" value="cloud_sync.asp">
<input type="hidden" name="next_host" value="">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_cloudsync">
<input type="hidden" name="action_wait" value="1">
<input type="hidden" name="cloud_sync" value="">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock">
				<table border="0" cellspacing="0" cellpadding="0">
					<tbody>
					<tr>
						<td>
							<a href="cloud_main.asp"><div class="tab"><span>AiCloud</span></div></a>
						</td>
						<td>
							<div class="tabclick"><span>Smart Sync</span></div>
						</td>
					</tr>
					</tbody>
				</table>
			</div>

		<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
						<tbody>
						<tr>
						  <td bgcolor="#4D595D" valign="top">

						<div>&nbsp;</div>
						<div class="formfonttitle">Cloud Sync</div>
						<div style="margin-left:5px;margin-top:10px;margin-bottom:10px"><img src="/images/New_ui/export/line_export.png"></div>
				
						<div>
							<table width="700px" style="margin-left:25px;">
								<tr>
									<td>
										<img id="guest_image" src="/images/cloudsync/cloudsync.png" width="100px">
									</td>
									<td>&nbsp;&nbsp;</td>
									<td>
										<span>The Quality of Service (QoS) ensures the network's speed performance. The default rule sets online gaming and web surfing as the highest priority and are not influenced by P2P applications (peer-to-peer applications such as BitTorrent). To enable QoS function, Click the QoS slide switch , and fill in the upload and download bandwidth fields. Get the bandwith information from your ISP.</span>
										<br/>
										<br/>
										<img src="/images/cloudsync/cloud_pic.png" width="400px" style="margin-left:50px;">
									</td>
								</tr>
							</table>
						</div>

   					<table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="cloudlistTable" style="margin-top:30px;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud List</td>
	   					</tr>
	  					</thead>		  

    					<tr>
      					<th width="10%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,2);">Provider</a></th>
    						<th width="25%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,3);">User Name</a></th>
      					<th width="10%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,2);">Rule</a></th>
      					<th width="30%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(18,3);">Folder</a></th>
      					<th width="15%">Status</th>
      					<th width="10%">Edit</th>
    					</tr>

						</table>
	
						<div id="cloud_synclist_Block"></div>

					  <table width="99%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3" class="FormTable" id="cloudAddTable" style="margin-top:10px;display:none;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud List</td>
	   					</tr>
	  					</thead>		  

							<tr>
							<th width="30%" style="height:40px;font-family: Calibri;font-weight: bolder;">
								Provider
							</th>
							<td>
								<div><img style="margin-top: -2px;" src="/images/cloudsync/ASUS-WebStorage.png"></div>
								<div style="font-size:18px;font-weight: bolder;margin-left: 45px;margin-top: -27px;font-family: Calibri;">ASUS WebStorage</div>
							</td>
							</tr>
				            
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#AiDisk_Account#>
							</th>			
							<td>
							  <input type="text" maxlength="32" class="input_32_table" style="height: 25px;" id="cloud_username" name="cloud_username" value="" onKeyPress="">
							</td>
						  </tr>	

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								<#AiDisk_Password#>
							</th>			
							<td>
							  <input type="text" maxlength="32" class="input_32_table" style="height: 25px;" id="cloud_password" name="cloud_password" value="" onKeyPress="">
							</td>
						  </tr>						  				
					  				
						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Directory
							</th>
							<td>
			          <input type="text" id="PATH" class="input_25_table" style="height: 25px;" name="cloud_dir" value="" onclick="location.href='#';showPanel();" readonly="readonly"/">
							</td>
						  </tr>

						  <tr>
							<th width="30%" style="font-family: Calibri;font-weight: bolder;">
								Rule
							</th>
							<td>
								<select name="cloud_rule" class="input_option">
									<option value="0">Sync</option>
									<option value="1">Download to USB Disk</option>
									<option value="2">Upload to Cloud</option>
								</select>			
							</td>
						  </tr>
						</table>	
					
   					<table width="98%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="cloudmessageTable" style="margin-top:7px;display:none;">
	  					<thead>
	   					<tr>
	   						<td colspan="6" id="cloud_synclist">Cloud Message</td>
	   					</tr>
	  					</thead>		  
							<tr>
								<td>
									<textarea style="width:99%; border:0px; font-family:'Courier New', Courier, mono; font-size:13px;background:#475A5F;color:#FFFFFF;" cols="63" rows="25" readonly="readonly" wrap=off ></textarea>
								</td>
							</tr>
						</table>

	  				<div class="apply_gen" id="creatBtn" style="margin-top:30px;display:none;">
							<input name="button" type="button" class="button_gen_long" onclick="isEdit=1;showAddTable();" value="<#AddAccountTitle#>"/>
							<img id="update_scan" style="display:none;" src="images/InternetScan.gif" />
	  				</div>

	  				<div class="apply_gen" style="margin-top:30px;display:none;" id="applyBtn">
	  					<input name="button" type="button" class="button_gen" onclick="isEdit=0;showAddTable();" value="<#CTL_Cancel#>"/>
	  					<input name="button" type="button" class="button_gen" onclick="applyRule()" value="<#CTL_ok#>"/>
	  				</div>

					  </td>
					</tr>				
					</tbody>	
				  </table>		
	
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
