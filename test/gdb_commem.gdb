set pagination off
set confirm off
break comMemReInitialize
run --info
print hp_com
print sizeof(struct comHdlType)
up
print cast_handle->com_hdl_front
print cast_handle->com_hdl_rear
print sizeof(*cast_handle)
quit
