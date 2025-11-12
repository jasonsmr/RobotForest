/**
 * Skip submodule staging if a pre-fetched runtime zip exists.
 * (pre-fetched via scripts/tools/fetch_runtime_release.sh)
 */
afterEvaluate {
  val rfZip = file("${project.projectDir}/src/main/assets/runtime/rf-runtime.zip")
  tasks.matching { it.name == "stageRuntimeZip" }.configureEach {
    onlyIf {
      if (rfZip.exists()) {
        logger.lifecycle("[rf-runtime] Pre-fetched rf-runtime.zip detected; skipping submodule staging (.kts)")
        false
      } else {
        true
      }
    }
  }
}
