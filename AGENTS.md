# Antigravity Autonomous Agent & Multi-Agent Orchestration Rules

## 1. Zero-Waste Token Protocol & Atomic Precision (`recursive-context-pruning-token-budgeting`)
- **Atomic Precision**: Eliminate filler phrases, conversational fluff, and unnecessary preambles. Start responses directly with actionable code, results, or concise architectural decisions.
- **Scoped Reads**: Never dump large files (>300 lines) into context unless strictly necessary. Use line-range parameters (`StartLine`, `EndLine`) to inspect only relevant functions/blocks.
- **Prompt Cache Alignment (`prompt-caching`)**: Maintain stable prompt prefixes to maximize KV-cache hits, reducing repeated input token costs by up to 90%.
- **Context Compression & Pruning (`context-compression`, `context-optimization`)**: When session history grows, summarize completed steps into compact state strings to prevent context degradation and token burnout.

## 2. Multi-Model Routing Hierarchy (`tokenwise`, `local-llm-expert`)
- **Tier 0 (Zero-Token / Local Offline)**: Run local models (Ollama: Qwen 2.5 Coder, DeepSeek-R1, Llama 3.3) via the configured MCP server (`ollama-local-models`) for 100% offline, zero-token cost tasks.
- **Tier 1 (Ultra-Low Token / High Speed)**: Use Gemini Flash / Flash-Lite for routine operations (file reading, formatting, unit test runs, git workflows, simple syntax fixes).
- **Tier 2 (Deep Reasoning / Flagship)**: Use Gemini 4 Argon / Gemini Pro strictly for multi-domain architectural decomposition, complex algorithm design, and security reviews.

## 3. Intelligent Task Evaluation & Skill Routing (`antigravity-skill-orchestrator`)
- **Simple / Contained Tasks**: Execute directly using standard file editing, search, and terminal tools without unnecessary skill or subagent overhead.
- **Complex / Multi-Domain Tasks**: Decompose using `concise-planning`, route to specialized skills in `.agents/skills` (or `~/.gemini/config/skills`), and orchestrate via subagents when tasks are independent.
- **Full Skill Library**: Backed by `C:\Users\chuno\.agents\skills` (1,860+ skills).

## 4. Multi-Agent & Subagent Orchestration (`subagent-driven-development`, `dispatching-parallel-agents`)
When handling large features, multi-file debugging, or independent subsystems:
1. **Define Clear Scopes**: Assign strict file/folder boundaries to each subagent so two agents never write to the same file simultaneously.
2. **Specialized Subagent Roles**:
   - `architect-planner`: System architecture, task decomposition, and Stage-1 Spec Compliance Review.
   - `fullstack-coder`: Python backend, APIs, Google Sheets bridges, and C++/ESP32 firmware implementation.
   - `ui-3d-specialist`: 3D WebGL/Three.js scenes (`water_map_3d.html`, `test_3d.html`), GCS dashboards, and UI/UX polish.
   - `qa-debugger`: Root-cause debugging (`systematic-debugging`), syntax/runtime validation (`lint-and-validate`), and Stage-2 Code Quality Review.
   - Built-in `research` / `self` / `image-generator` subagents for codebase exploration, isolated execution, and media generation.

## 5. Evidence-Based Quality Gates (`verification-before-completion`)
- Never claim a task is complete without verification evidence:
  - Python files: verify syntax (`python -m py_compile <file>`) or run tests.
  - JS/HTML files: verify scripts, DOM bindings, and check for console/syntax errors.
  - C++/PlatformIO files: verify build/include integrity.
- Preserve existing working features and comments unless explicitly asked to remove them.
