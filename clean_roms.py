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

def process_zip(zip_path, delete_zip=True, keep_regions=False):
    target_dir = os.path.dirname(zip_path)
    print(f"Processing: {os.path.basename(zip_path)}")
    
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # Get list of files in zip
            namelist = zip_ref.namelist()
            
            # Extract files
            for member in namelist:
                # Avoid directory entries
                if member.endswith('/') or os.path.basename(member) == '':
                    continue
                    
                # Extract file
                extracted_path = zip_ref.extract(member, target_dir)
                
                # Determine clean filename
                orig_filename = os.path.basename(extracted_path)
                cleaned_filename = clean_name(orig_filename, keep_regions)
                new_path = os.path.join(target_dir, cleaned_filename)
                
                # Handle directory structure if extracted path is nested
                if os.path.dirname(extracted_path) != target_dir:
                    # Move to target_dir
                    shutil.move(extracted_path, new_path)
                elif extracted_path != new_path:
                    # Rename in place
                    if os.path.exists(new_path):
                        print(f"  Warning: File already exists, renaming with suffix: {cleaned_filename}")
                        # append counter to handle duplicate names
                        base, ext = os.path.splitext(cleaned_filename)
                        counter = 1
                        while True:
                            collision_name = f"{base}_{counter}{ext}"
                            new_path = os.path.join(target_dir, collision_name)
                            if not os.path.exists(new_path):
                                break
                            counter += 1
                    os.rename(extracted_path, new_path)
                
                print(f"  Extracted & cleaned: {os.path.basename(new_path)}")
        
        # If there were subdirectories created during extraction, clean them up
        for member in namelist:
            if '/' in member:
                top_dir = member.split('/')[0]
                dir_to_remove = os.path.join(target_dir, top_dir)
                if os.path.exists(dir_to_remove) and os.path.isdir(dir_to_remove):
                    shutil.rmtree(dir_to_remove, ignore_errors=True)
                    
        # Delete zip file
        if delete_zip:
            os.remove(zip_path)
            print(f"  Deleted zip: {os.path.basename(zip_path)}")
            
    except Exception as e:
        print(f"  Error processing {os.path.basename(zip_path)}: {e}")

def main():
    parser = argparse.ArgumentParser(description="Clean ROM filenames by extracting ZIPs and cleaning parenthesis contents.")
    parser.add_argument("directory", nargs="?", default=".", help="Directory to scan (default: current directory)")
    parser.add_argument("--keep-regions", "-k", action="store_true", help="Do not remove region tags like (U), (USA), (J), (Europe), etc.")
    parser.add_argument("--keep-zip", action="store_true", help="Do not delete ZIP archives after extraction")
    args = parser.parse_args()

    target_dir = args.directory
        
    if not os.path.exists(target_dir):
        print(f"Error: Directory '{target_dir}' does not exist.")
        sys.exit(1)
        
    print(f"Scanning directory: {os.path.abspath(target_dir)}")
    zip_files = []
    for root, dirs, files in os.walk(target_dir):
        for file in files:
            if file.lower().endswith('.zip'):
                zip_files.append(os.path.join(root, file))
                
    if not zip_files:
        print("No .zip files found.")
        return
        
    print(f"Found {len(zip_files)} zip file(s).")
    for zip_file in zip_files:
        process_zip(zip_file, delete_zip=not args.keep_zip, keep_regions=args.keep_regions)
        
    print("Done!")

if __name__ == '__main__':
    main()
