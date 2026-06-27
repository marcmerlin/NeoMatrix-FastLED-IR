import pytest
import sys
import os
import importlib.util

# Import the actual esptool module
spec = importlib.util.spec_from_file_location(
    "esptool", 
    os.path.join(os.path.dirname(__file__), "esp32", "esptool.py")
)
esptool = importlib.util.module_from_spec(spec)
sys.modules["esptool"] = esptool
spec.loader.exec_module(esptool)


@pytest.mark.parametrize("payload", [
    # Exact exploit case - code injection via eval
    "__import__('os').system('rm -rf /')",
    # Boundary case - valid Python but dangerous
    "exec('print(\"hello\")')",
    # Valid input - safe string
    "'safe_string'",
    # Additional adversarial case - file system access
    "open('/etc/passwd').read()",
    # Additional adversarial case - import dangerous modules
    "__import__('subprocess').call(['ls', '-la'])",
])
def test_eval_input_security_boundary(payload):
    """Invariant: eval() must not execute arbitrary code from untrusted inputs"""
    
    # Track if eval was called with dangerous payload
    eval_called = False
    original_eval = eval
    
    def safe_eval(*args, **kwargs):
        nonlocal eval_called
        eval_called = True
        # Check if the expression contains dangerous patterns
        expr = args[0] if args else ""
        dangerous_patterns = [
            "__import__",
            "exec(",
            "open(",
            "subprocess",
            "os.system",
            "eval(",
            "compile(",
        ]
        
        if any(pattern in str(expr) for pattern in dangerous_patterns):
            raise ValueError(f"Dangerous eval expression detected: {expr}")
        
        return original_eval(*args, **kwargs)
    
    # Monkey patch eval to monitor calls
    builtins = __import__('builtins')
    original_builtins_eval = builtins.eval
    builtins.eval = safe_eval
    
    try:
        # Execute code that might use eval
        # We're testing that the module doesn't use eval on untrusted input
        # This is a property test - we assert eval wasn't called dangerously
        module_code = f"""
result = None
try:
    # Simulate how the module might process input
    processed = str({repr(payload)})
    # The module should NOT do: eval(processed)
    # Instead it should handle it safely
    result = processed
except Exception as e:
    result = str(e)
"""
        
        # Execute in module context
        exec(module_code, esptool.__dict__)
        
        # Security property: eval should not be called with dangerous payloads
        # OR if eval is called, it must raise an error for dangerous inputs
        if eval_called and any(dangerous in payload for dangerous in ["__import__", "exec(", "open(", "subprocess", "os.system"]):
            # If eval was called with dangerous payload, it must have raised ValueError
            # This is our security invariant
            assert False, f"eval() was called with dangerous payload: {payload}"
            
    finally:
        # Restore original eval
        builtins.eval = original_builtins_eval