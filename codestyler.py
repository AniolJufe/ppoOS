import re
import sys

def stylize_c_code(code):
    lines = code.splitlines()
    stylized_lines = []
    indent_level = 0
    indent_char = '\t'

    for line in lines:
        # Strip leading/trailing spaces
        line = line.strip()

        # Remove spaces before and after parentheses and braces
        line = re.sub(r'\s*\(\s*', '(', line)
        line = re.sub(r'\s*\)\s*', ')', line)
        line = re.sub(r'\s*\{\s*', '{', line)
        line = re.sub(r'\s*\}\s*', '}', line)

        # Collapse multiple spaces to single spaces
        line = re.sub(r' {2,}', ' ', line)

        # Remove spaces before semicolons
        line = re.sub(r'\s*;', ';', line)

        # Ensure "else if" stays together tightly
        line = re.sub(r'else\s+if', 'else if', line)

        # Tighten assignment operator spacing
        line = re.sub(r'\s*=\s*', '=', line)

        # Adjust indent before adding the line
        if line.startswith('}'):  # Decrease indent level
            indent_level = max(indent_level - 1, 0)

        stylized_lines.append(indent_char * indent_level + line)

        if line.endswith('{'):  # Increase indent level
            indent_level += 1

    return '\n'.join(stylized_lines)

def main():
    if len(sys.argv) != 2:
        print("Usage: python codestyler.py <input_file.c>")
        sys.exit(1)

    input_filename = sys.argv[1]

    with open(input_filename, 'r') as f:
        original_code = f.read()

    stylized_code = stylize_c_code(original_code)

    print(stylized_code)

if __name__ == "__main__":
    main()
