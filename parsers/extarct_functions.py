import re, sys, json, os
from collections import defaultdict

FUNC_DEF_RE = re.compile(
    r'^[\w\s\*]+?\s+([A-Za-z_]\w*)\s*\([^)]*\)\s*\{', re.MULTILINE
)
FUNC_CALL_RE = re.compile(r'\b([A-Za-z_]\w*)\s*\(')
ARRAY_REF_RE = re.compile(r'\b([A-Za-z_]\w*)\s*\[')

def parse_functions(source):
    """Return a dict: func_name -> func_body"""
    functions = {}
    for match in FUNC_DEF_RE.finditer(source):
        name = match.group(1)
        start = match.end()
        depth = 1
        i = start
        while i < len(source) and depth > 0:
            if source[i] == '{':
                depth += 1
            elif source[i] == '}':
                depth -= 1
            i += 1
        body = source[start:i]
        functions[name] = body
    return functions

def analyze_calls(functions):
    graph = {}
    for name, body in functions.items():
        calls = set(FUNC_CALL_RE.findall(body)) - {name}
        refs  = set(ARRAY_REF_RE.findall(body))
        graph[name] = {"calls": sorted(calls), "refs": sorted(refs)}
    return graph

def main(path):
    with open(path, encoding="latin-1") as f:
        source = f.read()

    functions = parse_functions(source)
    print(f"Parsed {len(functions)} functions")

    graph = analyze_calls(functions)
    print(f"Analyzed {len(graph)} call relationships")

    # Write JSON
    with open("call_graph.json", "w") as f:
        json.dump(graph, f, indent=2)
    print("Wrote call_graph.json")

    # Optional summary
    for fn, data in graph.items():
        if data["calls"]:
            print(f"\n{fn}() calls: {', '.join(data['calls'])}")
        if data["refs"]:
            print(f"  refs: {', '.join(data['refs'])}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python analyze_wlink.py wlinke.c")
        sys.exit(1)
    main(sys.argv[1])
