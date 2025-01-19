# Create Vivado Hardware project
1) Open Vivado
2) Write in the TCL Console:
      cd {C:/absolute/path/to}/Zybo-Z7-HW_files (this command will change the working directory where Vivado is executing the tcl commands)
      source Zybo-Z7-HW.tcl (this command will create the Vivado project, include sources,ips and build the Block Design)

Note: Be carefull with path length, Vivado does not like long path and it can lead to errors (especially with block design modules).

This will create a folder Zybo-Z7-HW containing the new Vivado project.

You can edit the Hardware in this project and then export the xsa file for Vitis by following the next steps:
1) Generate bitstream
2) Export Hardware files for Vitis (.xsa): File > Export > Export Hardware > Next > Include bitstream > Next > Finish
