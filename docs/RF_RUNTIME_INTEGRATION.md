
## RF Runtime Sync GitHub workflow

The Android app repo contains a workflow that keeps a local copy of the
\`rf-runtime-dev\` archive in sync with the runtime builder repository
(\`robotforest-wow64-runtime\`).

- Workflow file: \`.github/workflows/rf-sync-runtime.yml\`
- Workflow name: **RF Runtime Sync**
- Default source branch in the runtime repo:
  - \`ci/termux-safe-runtime\` (configurable via the
    \`runtime-branch\` input)

### What the workflow does

When triggered via **workflow_dispatch**:

1. Checks out the **RobotForest** repo on the requested branch
   (e.g. \`robotforest-wow64-runtime\`).
2. Installs \`jq\` for JSON parsing.
3. Runs \`scripts/runtime/rf_sync_runtime_ci.sh\`, which:
   - Queries the latest **successful** RF Release run in the
     \`robotforest-wow64-runtime\` repository:
     - Workflow file: \`rf-release.yml\`
     - Repository: \`jasonsmr/robotforest-wow64-runtime\`
   - Downloads the \`rf-runtime-dev\` artifact from that run using
     GitHub CLI (\`gh run download\`).
   - Places the resulting archive at:
     - \`scripts/runtime/rf-runtime-dev.tar.zst\`
4. The archive is **not committed** to git:
   - \`scripts/runtime/.gitignore\` ignores large/binary artifacts by
     default, while allowing the sync helper scripts to be tracked.

### When to use this workflow

- **Local development (CI-style):**
  - To refresh the runtime archive before running Android CI workflows
    that depend on \`rf-runtime-dev.tar.zst\`.
- **Release preparation:**
  - To ensure the app build is using a runtime that matches the latest
    green RF Release in the runtime builder repository.

For ad-hoc local development without GitHub Actions, developers can
also use:

- \`scripts/runtime/rf_sync_runtime_local.sh\`

which performs a similar sync directly from a local Termux shell,
writing the archive into the same path:

- \`scripts/runtime/rf-runtime-dev.tar.zst\`
