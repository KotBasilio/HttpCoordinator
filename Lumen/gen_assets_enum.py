#!/usr/bin/env python3
import re

def sanitize_name(path: str) -> str:
    """
    Convert a file path like './Assets/Icons/ic_user_16 px.png'
    into a valid C++ enum identifier: ICON_USER_16_PX
    """
    # Take just the filename without extension
    fname = path.split('/')[-1].split('\\')[-1]
    fname = fname.split('.')[0]

    # Replace spaces, dashes with underscores
    fname = re.sub(r'[\s\-]+', '_', fname)

    # Uppercase
    return fname.upper()

def main():
    with open("AssetList.txt", "r", encoding="utf-8") as f:
        lines = [line.strip() for line in f if line.strip()]

    enum_entries = []
    path_entries = []

    count = 0
    for line in lines:
        if line.startswith("./"):  # keep only actual asset paths
            ident = sanitize_name(line)
            enum_entries.append(f"    {ident},")
            path_entries.append(f"    \"{line}\",")
            count += 1

    with open("../ui/utils/AssetsEnum.h", "w", encoding="utf-8") as out:
        out.write("// Auto-generated from AssetList.txt with ./Assets/codegen.bat\n")
        out.write("#pragma once\n\n")
        out.write("enum AssetID {\n")
        out.write("\n".join(enum_entries))
        out.write("\n    ASSET_COUNT\n};\n\n")
        out.write("#ifdef COORD_ASSET_MAP_IMPL\n")
        out.write("static const char* AssetPaths[] = {\n")
        out.write("\n".join(path_entries))
        out.write("\n};\n")
        out.write("#endif // COORD_ASSET_MAP_IMPL\n")

    print(f"Written {count} enum lines")
        
if __name__ == "__main__":
    main()
