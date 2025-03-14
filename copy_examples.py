import shutil
import os

EXAMPLES_DIR = "examples"
DEST_DIR = "test/src"

# Ensure destination root directory exists
os.makedirs(DEST_DIR, exist_ok=True)

# Remove all files and subdirectories in test/examples before copying
for root, dirs, files in os.walk(DEST_DIR, topdown=False):
    for file in files:
        os.remove(os.path.join(root, file))
    for dir in dirs:
        shutil.rmtree(os.path.join(root, dir))

# Iterate over subdirectories in the examples folder
for example_name in os.listdir(EXAMPLES_DIR):
    example_path = os.path.join(EXAMPLES_DIR, example_name)
    
    # Check if it's a directory
    if os.path.isdir(example_path):
        # Ensure subdirectory in destination exists
        dest_subdir = os.path.join(DEST_DIR, example_name)
        os.makedirs(dest_subdir, exist_ok=True)

        # Find the .ino file inside the subdirectory
        for file in os.listdir(example_path):
            if file.endswith(".ino"):
                src_file = os.path.join(example_path, file)
                dest_file = os.path.join(dest_subdir, f"{example_name}.cpp")

                # Copy and rename .ino to .cpp
                shutil.copy(src_file, dest_file)
                print(f"Copied {src_file} -> {dest_file}")

