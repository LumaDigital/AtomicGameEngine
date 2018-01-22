@echo off
> _update_atomic_script_bindings.output.txt (

	:Solution
	echo =====================================
	echo Generate the script bindings (needed for adding components)
	echo =====================================
	Build\Windows\node\node.exe Build\node_modules\jake\bin\cli.js -f Build/Scripts/Bootstrap.js build:genscripts --trace

)
:End