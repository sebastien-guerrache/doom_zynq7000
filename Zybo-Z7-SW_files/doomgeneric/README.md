## Try DoomGeneric on Zybo-Z7

To try DoomGeneric on your Zybo-Z7 board, follow these steps:

### Step 1: Obtain a WAD File

To run DoomGeneric, you need a WAD file (game data). If you don't own the game, the shareware version (doom1.wad) is freely available.

1. **Download the Shareware WAD File**  
   - Download the shareware version of the WAD file (doom1.wad) from a reputable source online.

### Step 2: Convert the WAD File to C Source Code

1. **Place the WAD File**  
   - Place the downloaded `doom1.wad` file into the `doomgeneric` directory within your project.
2. **Convert the WAD File**  
   - Open Git Bash in the `doomgeneric` directory.
   - Use the following command to convert the `doom1.wad` file to a C header file:
      ```sh
      xxd -i doom1.wad > DOOM_WAD.c
      ```
