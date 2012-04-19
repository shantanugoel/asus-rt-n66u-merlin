<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title>ASUS Wireless Router Web Manager</title>
<link rel="stylesheet" type="text/css" href="other.css">

<script>
var reboot_needed_time = 60;
var action_mode = '<% get_parameter("action_mode"); %>';
function redirect(){
	setTimeout("redirect1();", reboot_needed_time*1000);
}

function redirect1(){
	if(action_mode == "reboot"){
		parent.location.href = "/"	;
		return false;
	}else if(parent.lan_ipaddr == "192.168.1.1"){
			parent.parent.location.href = "http://192.168.1.1/QIS_wizard.htm?flag=welcome";
	}else{
		parent.$('drword').innerHTML = "<#Setting_factorydefault_iphint#><br/>";
		setTimeout("parent.hideLoading()",1000);
		setTimeout("parent.dr_advise();",1000);
		parent.parent.location.href = "http://192.168.1.1/QIS_wizard.htm?flag=welcome";
	}
}
</script>
</head>

<body onLoad="redirect();">
<script>
parent.hideLoading();
parent.showLoading(reboot_needed_time, "waiting");
</script>
</body>
</html>