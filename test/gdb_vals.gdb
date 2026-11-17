set pagination off
set confirm off
set print elements 80
break Valsfile.cpp:473
run --info --preset upstream/presets/1.fac
print cast_handle->file_version
print cast_handle->wcp_comment
print cast_handle->total_num_elements
print num_ints
print num_reals
print num_strings
print i_num_bands
print wcp_str
quit
