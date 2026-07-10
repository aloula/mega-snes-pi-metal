#!/usr/bin/env python3
import os
import sys
import zipfile
import re
import shutil
import argparse

def clean_name(name, keep_regions=False):
    # Split name and extension
    base, ext = os.path.splitext(name)
    
    if keep_regions:
        # Helper to check if the parentheses contents match standard region names/codes
        def is_region(text):
            # Split by common separators (spaces, commas, slashes, dashes)
            parts = re.split(r'[\s,/\-\+]+', text.strip())
            region_words = {
                'u', 'usa', 'j', 'japan', 'e', 'europe', 'w', 'world', 'a', 'australia', 
                'f', 'france', 'g', 'germany', 'i', 'italy', 's', 'spain', 'k', 'korea', 
                'ch', 'china', 'asia', 'brazil', 'canada', 'nl', 'netherlands', 'sweden', 
                'sw', 'pd', 'public domain', 'ju', 'ue', 'eu', 'world'
            }
            # If all sub-parts are recognized region tokens, we treat the tag as a region
            return all(p.lower() in region_words for p in parts if p)

        # Regex replacement callback
        def repl(match):
            content = match.group(1)
            if is_region(content):
                return match.group(0) # Keep region tag and its preceding spaces/parentheses
            return "" # Strip non-region tag

        # Match any pattern like " (contents)" or "(contents)"
        new_base = re.sub(r'\s*\(([^)]*)\)', repl, base)
    else:
        # Standard: remove anything inside parenthesis and any leading space before it
        new_base = re.sub(r'\s*\([^)]*\)', '', base)
        
    # Replace multiple spaces with a single space and strip
    new_base = re.sub(r'\s+', ' ', new_base).strip()
    return new_base + ext

def get_all_files(directory):
    files_set = set()
    for root, _, files in os.walk(directory):
        for file in files:
            files_set.add(os.path.abspath(os.path.join(root, file)))
    return files_set

def extract_7z(archive_path, target_dir):
    # Try importing py7zr first (pip install py7zr)
    try:
        import py7zr
        with py7zr.SevenZipFile(archive_path, mode='r') as archive:
            namelist = archive.getnames()
            archive.extractall(path=target_dir)
            # Return absolute paths of extracted files/directories
            return [os.path.abspath(os.path.join(target_dir, name)) for name in namelist]
    except ImportError:
        pass

    # Fallback to system command line utilities: 7z, 7za, 7zr
    import subprocess
    files_before = get_all_files(target_dir)
    
    for cmd in ['7z', '7za', '7zr']:
        try:
            # 7z syntax: 7z x archive.7z -o/target/dir -y
            result = subprocess.run([cmd, 'x', archive_path, f'-o{target_dir}', '-y'],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            if result.returncode == 0:
                files_after = get_all_files(target_dir)
                # Return the absolute paths of all new files created
                return list(files_after - files_before)
        except FileNotFoundError:
            continue

    raise RuntimeError(
        "Could not extract .7z file. Please install either the 'py7zr' python library "
        "(pip install py7zr) or the system utility 'p7zip' (e.g. sudo apt install p7zip-full)."
    )

def process_archive(archive_path, delete_archive=True, keep_regions=False):
    target_dir = os.path.dirname(archive_path)
    print(f"Processing archive: {os.path.basename(archive_path)}")
    
    is_zip = archive_path.lower().endswith('.zip')
    extracted_paths = []
    
    try:
        if is_zip:
            with zipfile.ZipFile(archive_path, 'r') as zip_ref:
                namelist = zip_ref.namelist()
                for member in namelist:
                    if member.endswith('/') or os.path.basename(member) == '':
                        continue
                    extracted_path = zip_ref.extract(member, target_dir)
                    extracted_paths.append(os.path.abspath(extracted_path))
        else:
            extracted_paths = extract_7z(archive_path, target_dir)
            
        # Clean extracted files
        for ext_path in extracted_paths:
            if not os.path.exists(ext_path) or os.path.isdir(ext_path):
                continue
                
            orig_filename = os.path.basename(ext_path)
            cleaned_filename = clean_name(orig_filename, keep_regions)
            new_path = os.path.join(target_dir, cleaned_filename)
            
            # Handle directory structure if extracted path is nested
            if os.path.dirname(ext_path) != os.path.abspath(target_dir):
                if os.path.exists(new_path):
                    print(f"  Removing duplicate extracted game: {orig_filename} (already have {cleaned_filename})")
                    os.remove(ext_path)
                else:
                    shutil.move(ext_path, new_path)
                    print(f"  Extracted & cleaned: {cleaned_filename}")
            elif os.path.abspath(ext_path) != os.path.abspath(new_path):
                if os.path.exists(new_path):
                    print(f"  Removing duplicate extracted game: {orig_filename} (already have {cleaned_filename})")
                    os.remove(ext_path)
                else:
                    os.rename(ext_path, new_path)
                    print(f"  Extracted & cleaned: {cleaned_filename}")
            else:
                print(f"  Extracted & cleaned: {cleaned_filename}")
            
        # Clean up any nested directories created during extraction
        for ext_path in extracted_paths:
            relative_part = os.path.relpath(ext_path, target_dir)
            if '/' in relative_part:
                top_dir = relative_part.split('/')[0]
                dir_to_remove = os.path.join(target_dir, top_dir)
                if os.path.exists(dir_to_remove) and os.path.isdir(dir_to_remove):
                    shutil.rmtree(dir_to_remove, ignore_errors=True)
                    
        # Delete archive file
        if delete_archive:
            os.remove(archive_path)
            print(f"  Deleted archive: {os.path.basename(archive_path)}")
            
    except Exception as e:
        print(f"  Error processing {os.path.basename(archive_path)}: {e}")

def clean_existing_roms(target_dir, keep_regions=False):
    print("Scanning for existing unzipped ROM files to clean up...")
    rom_extensions = ('.sfc', '.smc', '.nes', '.md', '.gen', '.bin', '.pce', '.cue', '.chd')
    cleaned_count = 0
    duplicate_count = 0
    
    for root, _, files in os.walk(target_dir):
        for file in files:
            if file.lower().endswith(rom_extensions):
                current_path = os.path.join(root, file)
                cleaned_filename = clean_name(file, keep_regions)
                new_path = os.path.join(root, cleaned_filename)
                
                if os.path.abspath(current_path) == os.path.abspath(new_path):
                    continue
                    
                if os.path.exists(new_path):
                    print(f"  Removing duplicate game: {file} (already have {cleaned_filename})")
                    try:
                        os.remove(current_path)
                        duplicate_count += 1
                    except Exception as e:
                        print(f"  Error removing duplicate {file}: {e}")
                else:
                    print(f"  Cleaning name: {file} -> {cleaned_filename}")
                    try:
                        os.rename(current_path, new_path)
                        cleaned_count += 1
                    except Exception as e:
                        print(f"  Error renaming {file}: {e}")
                        
    print(f"Cleanup of existing ROMs complete! Renamed {cleaned_count} files, removed {duplicate_count} duplicate files.")

def main():
    parser = argparse.ArgumentParser(description="Clean ROM filenames by extracting ZIP/7Z archives, cleaning parenthesis contents, and removing duplicate games.")
    parser.add_argument("directory", nargs="?", default=".", help="Directory to scan (default: current directory)")
    parser.add_argument("--keep-regions", "-k", action="store_true", help="Do not remove region tags like (U), (USA), (J), (Europe), etc.")
    parser.add_argument("--keep-archive", action="store_true", help="Do not delete archives after extraction")
    args = parser.parse_args()

    target_dir = args.directory
        
    if not os.path.exists(target_dir):
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)
        
    print(f"Scanning directory: {os.path.abspath(target_dir)}")
    
    # 1. Process all archives (.zip, .7z)
    archive_files = []
    for root, dirs, files in os.walk(target_dir):
        for file in files:
            if file.lower().endswith(('.zip', '.7z')):
                archive_files.append(os.path.join(root, file))
                
    if archive_files:
        print(f"Found {len(archive_files)} archive file(s).")
        for archive_file in archive_files:
            process_archive(archive_file, delete_archive=not args.keep_archive, keep_regions=args.keep_regions)
    else:
        print("No .zip or .7z archives found.")
        
    # 2. Clean existing unzipped ROM files
    clean_existing_roms(target_dir, keep_regions=args.keep_regions)
        
    print("Done!")

if __name__ == '__main__':
    main()
