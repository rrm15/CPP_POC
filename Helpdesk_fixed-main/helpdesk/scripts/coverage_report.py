#!/usr/bin/env python3
import glob
import os
import sys

def main():
    cov_dir = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] else 'build-coverage'
    if not os.path.exists(cov_dir):
        cov_dir = 'build'

    objs_dir = os.path.join(cov_dir, 'CMakeFiles', 'helpdesk_core.dir', 'src')
    cpp_files = sorted(glob.glob('src/*.cpp'))
    results = []

    for cpp in cpp_files:
        base = os.path.basename(cpp)
        gcda = os.path.join(cov_dir, 'CMakeFiles', 'helpdesk_core.dir', 'src', base + '.gcda')
        os.system(f"gcov -o {gcda} {cpp} > /dev/null 2>&1")
        gcov_file = base + ".gcov"
        executed = 0
        total = 0
        if os.path.exists(gcov_file):
            with open(gcov_file, 'r', encoding='utf-8', errors='ignore') as f:
                for line in f:
                    parts = line.split(':', 2)
                    if len(parts) >= 2:
                        cov_str = parts[0].strip()
                        if cov_str == '#####':
                            total += 1
                        elif cov_str.isdigit():
                            if int(cov_str) > 0:
                                executed += 1
                            total += 1
            try:
                os.remove(gcov_file)
            except OSError:
                pass
        pct = (executed / total * 100.0) if total > 0 else 0.0
        results.append((base, executed, total, pct))

    print("=" * 60)
    print("HELPDESK COVERAGE REPORT")
    print("=" * 60)
    print(f"{'SOURCE FILE':<32} {'EXECUTED':>8} {'TOTAL':>6} {'COVERAGE':>10}")
    print("-" * 60)

    total_exec = sum(r[1] for r in results)
    total_lines = sum(r[2] for r in results)
    weighted_total = (total_exec / total_lines * 100.0) if total_lines > 0 else 0.0
    simple_avg = (sum(r[3] for r in results) / len(results)) if results else 0.0

    for name, exec_cnt, tot_cnt, pct in results:
        print(f"{name:<32} {exec_cnt:>8} {tot_cnt:>6} {pct:>9.2f}%")

    print("-" * 60)
    print(f"{'WEIGHTED TOTAL':<32} {total_exec:>8} {total_lines:>6} {weighted_total:>9.2f}%")
    print(f"{'SIMPLE AVERAGE':<32} {'':>8} {'':>6} {simple_avg:>9.2f}%")
    print("=" * 60)

    rep_dir = os.path.join('build', 'reports')
    os.makedirs(rep_dir, exist_ok=True)
    csv_path = os.path.join(rep_dir, 'coverage.csv')
    with open(csv_path, 'w', encoding='utf-8') as f:
        f.write("Source File,Executed Lines,Total Lines,Coverage Percent\n")
        for name, exec_cnt, tot_cnt, pct in results:
            f.write(f"{name},{exec_cnt},{tot_cnt},{pct:.2f}%\n")
        f.write(f"WEIGHTED TOTAL,{total_exec},{total_lines},{weighted_total:.2f}%\n")
        f.write(f"SIMPLE AVERAGE,,,{simple_avg:.2f}%\n")

    print(f"Coverage CSV: {csv_path}")

    # Clean up any leftover *.gcov files in current directory
    for gf in glob.glob("*.gcov"):
        try:
            os.remove(gf)
        except OSError:
            pass

if __name__ == '__main__':
    main()
