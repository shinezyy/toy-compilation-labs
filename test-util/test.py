import sh
from os import path
import glob

with open('test_path.txt') as f:
    testcase_path = f.read().replace('\n', '')
testcase_path = path.expanduser(testcase_path)
c_files = glob.glob(path.join(testcase_path, '*.c'))
c_files = sorted(c_files)
opt_files = [x.replace('.c', '.opt') for x in c_files]


def parse_line(line: str):
    line_number, functions = line.split(':')
    line_number = int(line_number.replace('/', ''))
    functions = set([func.strip() for func in functions.split(',')])
    return line_number, functions


def get_expected_output(source_file):
    with open(source_file) as f:
        lines = f.read().split('\n')
    expected = {}
    for line in lines:
        if line.startswith('//'):
            line_number, functions = parse_line(line)
            expected[line_number] = functions
    return expected


def compare(c_file, opt):
    # ensure opt files have been generated
    assert path.isfile(opt)
    print('-' * 80)
    print(opt)
    output = str(sh.llvmassignment(opt).stderr).strip("'")  # Lab2 is used for testing
    # output = str(sh.dataflow(opt).stderr).strip("'")  # Lab3
    lines = output.split('\\n')[1:]  # if output filename, first line should be deleted
    expected = get_expected_output(c_file)
    for line in lines:
        line_number, functions = parse_line(line)
        if line_number not in expected:
            print('Unexpected line number: {}'.format(line))
            print('Expected: {}'.format(expected))
            return False
        if expected[line_number] != functions:
            print('Results: {}'.format(functions))
            print('Expected: {}'.format(expected[line_number]))
            return False
    print('OK')
    return True


def main():
    for c_file, opt in zip(c_files, opt_files):
        compare(c_file, opt)
        break


if __name__ == '__main__':
    main()
