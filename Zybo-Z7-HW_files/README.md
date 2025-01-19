# Create Vivado Hardware Project for Zybo-Z7

Follow the steps below to set up your Vivado hardware project:

## Step 1: Open Vivado

## Step 2: Run TCL Commands

In the **TCL Console**, execute the following commands:

```tcl
cd {C:/absolute/path/to}/Zybo-Z7-HW_files
```
- This command changes the working directory to the folder where the TCL commands will execute.

```tcl
source Zybo-Z7-HW.tcl
```
- This command creates the Vivado project, adds sources, IPs, and builds the block design.

> **Note:**  
> Be cautious with the path length. Vivado has limitations with long paths, especially for block design modules, which may result in errors. Use shorter paths if you encounter issues.

## Step 3: Locate the Generated Project

After running the above commands, a folder named `Zybo-Z7-HW` will be created. This folder contains the new Vivado project.

---

## Editing Hardware and Exporting for Vitis

Once your project is created, you can edit the hardware design and export the necessary `.xsa` file for Vitis. Follow these steps:

1. **Generate the Bitstream**  
   - In Vivado, click **Generate Bitstream**.

2. **Export Hardware Files for Vitis**  
   - Go to **File** > **Export** > **Export Hardware**.  
   - In the dialog:
     - **Include Bitstream**: Check this option.
   - Click **Next**, then **Finish**.

---

This `.xsa` file can now be used with Vitis to develop software for the Zybo-Z7 platform.
