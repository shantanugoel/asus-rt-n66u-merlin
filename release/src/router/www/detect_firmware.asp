var webs_state_update= '<% nvram_get("webs_state_update"); %>';
var webs_state_error= '<% nvram_get("webs_state_error"); %>';
var webs_state_info= '<% nvram_get("webs_state_info"); %>';
var usb_path1_index = '<% nvram_get("usb_path1"); %>';
var usb_path2_index = '<% nvram_get("usb_path2"); %>';
<% disk_pool_mapping_info(); %>
<% available_disk_names_and_sizes(); %>

