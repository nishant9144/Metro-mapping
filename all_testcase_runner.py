import os
import subprocess
import tempfile
import time
import sys
import threading
import queue
import signal

def ensure_executable(exe, source):
    if not os.path.exists(exe):
        print(f"Compiling {exe} from {source} ...")
        res = subprocess.run(["g++", "-std=c++17", "-O2", "-o", exe, source])
        if res.returncode != 0:
            print(f"Failed to compile {exe} from {source}")
            sys.exit(1)
        print(f"Compiled {exe}.")

def ensure_all_binaries():
    ensure_executable("./encoder", "encoder.cpp")
    ensure_executable("./decoder", "decoder.cpp")

def load_testcases(folder):
    cases = {}
    for fname in os.listdir(folder):
        if fname.endswith(".city"):
            with open(os.path.join(folder, fname), "r") as f:
                cases[fname] = f.read()
    return cases

# --- Load your testcases from folders ---
SAT_CASES = load_testcases("testcases_sat")
UNSAT_CASES = load_testcases("testcases_unsat")

def run_case(case_name, city_content, expect_sat, temp_dir):
    city_path = os.path.join(temp_dir, case_name)
    base = os.path.splitext(city_path)[0]
    with open(city_path, "w") as f:
        f.write(city_content)
    # Run encoder
    res = subprocess.run(["./encoder", base], capture_output=True)
    if res.returncode != 0:
        print(f"[{case_name}] Encoder failed:\n{res.stderr.decode()}")
        return False
    # Run minisat
    res = subprocess.run(["minisat", base + ".satinput", base + ".satoutput"], capture_output=True)
    if res.returncode not in (10, 20):
        print(f"[{case_name}] Minisat failed:\n{res.stderr.decode()}")
        return False
    # Run decoder
    res = subprocess.run(["./decoder", base], capture_output=True)
    if res.returncode != 0:
        print(f"[{case_name}] Decoder failed:\n{res.stderr.decode()}")
        return False
    # Run format_checker
    res = subprocess.run(["python3", "format_checker.py", base], capture_output=True)
    output = res.stdout.decode()
    # Check result
    if expect_sat:
        passed = "FINAL VERDICT: VALID" in output
    else:
        passed = "UNSAT" in output
    if not passed:
        print(f"[{case_name}] FAILED\nOutput:\n{output}")
    return passed

def testcase_sort_key(x):
    digits = ''.join(filter(str.isdigit, x))
    return (int(digits), x) if digits else (float('inf'), x)

def input_listener(q):
    try:
        if os.name == 'nt':
            # Windows
            import msvcrt
            while True:
                if msvcrt.kbhit():
                    ch = msvcrt.getch()
                    if ch in (b't', b'T'):
                        q.put('T')
        else:
            # Unix-like
            import sys, tty, termios
            fd = sys.stdin.fileno()
            old_settings = termios.tcgetattr(fd)
            try:
                tty.setcbreak(fd)
                while True:
                    ch = sys.stdin.read(1)
                    if ch.lower() == 't':
                        q.put('T')
            finally:
                termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    except Exception:
        pass

def run_case_with_terminate(case_name, city_content, expect_sat, temp_dir):
    q = queue.Queue()
    listener = threading.Thread(target=input_listener, args=(q,), daemon=True)
    listener.start()
    result = [None]

    def testcase_runner():
        result[0] = run_case(case_name, city_content, expect_sat, temp_dir)

    t = threading.Thread(target=testcase_runner)
    t.start()
    while t.is_alive():
        t.join(timeout=0.1)
        try:
            if not q.empty() and q.get_nowait() == 'T':
                print(f"\nTestcase '{case_name}' terminated by user.")
                return None, True
        except queue.Empty:
            pass
    return result[0], False

def main():
    ensure_all_binaries()
    start_time = time.time()
    total = 0
    passed = 0
    print("="*40)
    print("Press 'T' at any time to terminate the current testcase and move to the next.")
    with tempfile.TemporaryDirectory() as temp_dir:
        print("="*40)
        print("Running SAT cases...")
        print("="*40)
        for idx, name in enumerate(sorted(SAT_CASES.keys(), key=testcase_sort_key)):
            content = SAT_CASES[name]
            total += 1
            t0 = time.time()
            ok, terminated = run_case_with_terminate(name, content, expect_sat=True, temp_dir=temp_dir)
            t1 = time.time()
            if terminated:
                status = "TERMINATED"
            else:
                status = "PASS" if ok else "FAIL"
                if ok: passed += 1
            print(f"[{idx+1:02d}] {name:<15} | {status} | Time: {t1-t0:.2f}s")
        print("="*40)
        print("Running UNSAT cases...")
        print("="*40)
        for idx, name in enumerate(sorted(UNSAT_CASES.keys(), key=testcase_sort_key)):
            content = UNSAT_CASES[name]
            total += 1
            t0 = time.time()
            ok, terminated = run_case_with_terminate(name, content, expect_sat=False, temp_dir=temp_dir)
            t1 = time.time()
            if terminated:
                status = "TERMINATED"
            else:
                status = "PASS" if ok else "FAIL"
                if ok: passed += 1
            print(f"[{idx+1:02d}] {name:<15} | {status} | Time: {t1-t0:.2f}s")
        print("="*40)
    elapsed = time.time() - start_time
    print(f"\nSummary: {passed}/{total} testcases passed in {elapsed:.2f} seconds.")

if __name__ == "__main__":
    main()