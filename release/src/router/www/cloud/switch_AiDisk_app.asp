<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">

<script>
var getprotocol = '<% get_parameter("protocol"); %>';

function set_AiDisk_status_error(error_msg){
	alert(error_msg);
	parent.resultOfSwitchAppStatus(error_msg);
}

function set_AiDisk_status_success(){
	if(getprotocol == "ftp"){
		parent.resultOfSwitchAppStatus();
	}
	else if(getprotocol == "webdav"){
		parent.resultOfSwitchWebdavStatus();
	}
}

function set_share_mode_error(error_msg){
	parent.alert_error_msg(error_msg);
}

function set_share_mode_success(){
	return true;
}
</script>
</head>

<body onload="set_AiDisk_status_success();">

<% set_AiDisk_status(); %>
<% set_share_mode(); %>

</body>
</html>
