function hello()
	dad("heldi", "new")
	dad("heldi", "label", "Hello world!")
	dad("heldi", "button_closes", "close", 0)
	dad("heldi", "run_modal", "hello-world", "Hello world dialog")
end function

fgw_func_reg(hello)

