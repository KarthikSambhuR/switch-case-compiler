# Switch Statement Compiler 

A compiler front-end for C-style `switch`, `case`, `default`, and `break` statements. The project performs lexical analysis, syntax analysis, AST construction, compile-time constant expression evaluation, case-label syntax validation, and duplicate constant case label detection. It is validation-only: it validates syntax and semantics without needing code execution.

## System Architecture

| Layer | Technology | Entry point |
| --- | --- | --- |
| Compiler | Flex scanner, Bison LALR(1) grammar, C11 AST & semantic engine | `compiler/bin/switchc.exe` |
| Bridge | Python 3 standard-library HTTP server | `backend/server.py` |
| Frontend | Modern interactive UI (HTML5, Vanilla CSS, JavaScript) | `frontend/index.html` |

The browser sends code to the local Python bridge, which executes the native compiler with `--json` and returns the token stream, syntax status, AST, diagnostics, and switch scope analysis.

## Core Features

- **Lexical Analysis (Flex)**:
  - Keywords: `switch`, `case`, `default`, `break`, `int`, `char`, `float`, `void`
  - Literals: decimal integers (`10`), hex (`0x1F`), octal (`077`), character constants (`'A'`, `'\n'`), strings, identifiers
  - Operators & Delimiters: `(`, `)`, `{`, `}`, `:`, `;`, `=`, arithmetic, comparison, logical, bitwise operators
  - Line and column position tracking on every token
  - Lexical error detection: illegal characters (`L001`), unterminated literals (`L002`), unclosed comments (`L003`)

- **Syntax Analysis (Bison LALR(1))**:
  - `switch ( expression ) statement`
  - `case constant_expression : statement`
  - `default : statement`
  - `break ;`
  - Block compounds `{ ... }`, expression statements, assignments, and variable declarations
  - Precedence and associativity for arithmetic, relational, bitwise, and logical expressions

- **Compile-Time Constant Expression Evaluation**:
  - Evaluates integer literals, character literals (ASCII), and arithmetic/bitwise/relational constant expressions (e.g., `2 + 3` evaluates to `5`, `(1 << 4)` evaluates to `16`, `'A'` evaluates to `65`)
  - Detects division by zero and modulo by zero during constant evaluation

- **Semantic Validation**:
  - **Case-label syntax validation**: Verifies that every case label is a compile-time constant integer expression. Detects non-constant identifiers (`S001`), floating-point values (`S001`/`S002`), strings, and invalid expressions.
  - **Duplicate constant case labels detection**: Tracks constant case labels within each switch statement scope. Detects duplicate evaluated values (e.g. `case 5:` and `case 2 + 3:`, or `case 'A':` and `case 65:`) and reports the duplicate along with the previous declaration line and column (`S004`).
  - **Multiple default labels**: Flags multiple `default:` labels within the same switch statement (`S005`).
  - **Contextual break validation**: Validates that `break` statements are located within a valid switch statement scope (`S006`).
  - **Misplaced labels**: Detects `case` and `default` labels placed outside of any switch statement (`S007`).
  - **Switch expression type checking**: Ensures the switch condition expression evaluates to an integral type (`S008`).
  - **Nested switch scoping**: Maintains isolated case label tables for nested switch statements so inner switch cases do not clash with outer switches.

## Diagnostics Reference

| Code | Stage | Description |
| --- | --- | --- |
| `L001` | Lexical | Illegal character encountered |
| `L002` | Lexical | Unterminated character or string literal |
| `L003` | Lexical | Unterminated block comment |
| `P001` | Syntax | Bison syntax parse error (e.g. missing colon, missing condition) |
| `S001` | Semantic | Case label is not a compile-time constant expression |
| `S004` | Semantic | Duplicate constant case label in switch statement |
| `S005` | Semantic | Multiple `default` labels in switch statement |
| `S006` | Semantic | `break` statement not within switch statement |
| `S007` | Semantic | `case` or `default` label outside switch statement |
| `S008` | Semantic | Switch expression is not an integral type |

## Requirements

- Windows with MinGW GCC on `PATH`
- WSL with `bison` and `flex` installed
- Python 3.10 or newer
- Modern web browser

## Quick Start

### 1. Build the Compiler

```powershell
powershell -ExecutionPolicy Bypass -File compiler\build.ps1
```

### 2. Run Test Suite

```powershell
python tests\run_tests.py --verbose
python tests\test_bridge.py
```

### 3. Launch Web Interface

```powershell
python backend\server.py --open
```

Open `http://127.0.0.1:8000/` in your browser.

### 4. Compiler CLI

```powershell
# Analyze inline switch statement
compiler\bin\switchc.exe -e "switch (x) { case 1: break; case 1: break; }"

# Analyze example files
compiler\bin\switchc.exe examples\valid_switch.c
compiler\bin\switchc.exe examples\duplicate_evaluated.c

# Inspect token stream
compiler\bin\switchc.exe --tokens -e "switch (x) { case 'A': break; }"

# Emit JSON contract
compiler\bin\switchc.exe --json -e "switch (x) { case 1: break; default: break; }"
```
