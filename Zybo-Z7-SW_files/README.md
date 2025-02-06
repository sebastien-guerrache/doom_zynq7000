## Create Vitis Classic Software Project for Zybo-Z7

> **Note:**  
> Please refer to the ReadMe.md in the doomgeneric directory to be fully prepared for the following actions

Follow the steps below to set up your Vitis Classic project:

## Step 1: Open Vitis Classic

> **Note:**  
> Ensure you have Vitis Classic 2024.1 or later installed.

## Step 2: Create a New Application Project

1. **Launch Vitis Classic**  
   - Open Vitis Classic and select your workspace directory.

2. **Create a New Application Project**  
   - Go to **File** > **New** > **Application Project**.
   - Click **Next**.

3. **Select the Hardware Platform**  
   - Click on the **Create a new platform from hardware (XSA)** tab.
   - Click **Browse** and select the `.xsa` file exported from Vivado.
   - Click **Next**.

4. **Select a System Project**  
   - Choose an **Application project name** and click **Next**.
   - Ensure **ps7_cortexa9_0** is selected by default.

5. **Select a Domain**  
   - Ensure the operating system is set to **standalone** and click **Next**.

6. **Select a Template**  
   - Select **Empty Application(C)** to start from scratch.
   - Click **Finish**.

## Step 3: Import Source Files

1. **Add Source Files**  
   - Right-click on the **src** folder in your project and select **Import Sources**.
   - Browse to the location of your folder containing the source files and import them.

2. **Build the Project**  
   - Click **Project** > **Build Project** to compile your application.

## Step 4: Run the Application

1. **Connect the Zybo-Z7 Board**  
   - Connect your Zybo-Z7 board to your computer via USB.

2. **Run the Application**  
   - Right-click on your application project and select **Run As** > **Launch on Hardware (System Debugger)**.


Your application should now be running on the Zybo-Z7 platform. Enjoy!


## Bonus: Open Terminal for Debugging in Vitis Classic

1. **Open Terminal**  
   - Go to **Window** > **Show View** > **Terminal**.
   - If the Terminal view is not listed, go to **Window** > **Show View** and search for **Terminal**.

2. **Configure Terminal**  
   - In the Terminal view, click the **Open a Terminal** icon or press **Ctrl+Shift+Alt+T**.
   - Choose **Serial Terminal** as the connection type and configure the **Baud Rate** to 115200.

3. **Use Terminal for Debugging**  
   - Use the terminal to interact with your application for debugging purposes.








