-- Include AFTER lib/CommonLibSSE-NG. Keep upstream headers/rules/options intact.
-- The cache is an optional acceleration: an absent entry builds from source.
option("shared_commonlib", function()
    set_default(true)
    set_description("Reuse immutable, configuration-matched local CommonLib builds")
end)
option("commonlib_cache_dir", function()
    local directory = os.getenv("COMMONLIB_CACHE_DIR")
    -- Existing workspace caches also work in already-open shells that have not
    -- inherited a newly configured user environment variable yet.
    local ancestor = os.projectdir()
    while not directory do
        local candidate = path.join(ancestor, ".build-cache", "commonlib")
        if os.isdir(candidate) then directory = candidate; break end
        local parent = path.directory(ancestor)
        if not parent or parent == "" or #parent >= #ancestor then break end
        ancestor = parent
    end
    set_default(directory or path.join(os.getenv("LOCALAPPDATA") or os.tmpdir(), "xmake", "commonlib"))
    set_description("Shared CommonLib binary cache directory")
end)

target("commonlibsse-ng", function()
    -- Own the source/prebuilt selection; do not mix upstream all-runtime bundles
    -- with the local SE+AE configuration. No changes inside the submodule.
    on_load(function(target)
        target:set("kind", "static")
        target:set("pcxxheader", path.join(target:scriptdir(), "include", "SKSE", "Impl", "PCH.h"))
        -- Canonicalize __FILE__ in the shared library for all consumers.
        target:add("cxxflags", "cl::/d1trimfile:" .. os.projectdir() .. "\\", {force = true})
    end)
    on_config(function() end)
    add_rules("shared.commonlib")
end)

rule("shared.commonlib", function()
    add_deps("mode.debug", "mode.releasedbg")
    after_config(function(target)
        import("core.project.config")
        import("core.base.json")
        import("core.base.bytes")
        if not has_config("shared_commonlib") or has_config("tests") then return end
        local source = target:scriptdir()
        local revision = try { function() return os.iorunv("git", {"-C", source, "rev-parse", "HEAD"}):trim() end }
        local dirty = try { function() return os.iorunv("git", {"-C", source, "status", "--porcelain", "--untracked-files=all"}):trim() end }
        if not revision or dirty ~= "" then
            cprint("${yellow}CommonLib cache: source checkout is unavailable or modified; compiling locally")
            return
        end
        local compiler = target:compiler("cxx")
        local program = compiler:program()
        if path.filename(program):lower() ~= "cl.exe" then return end
        local flags = {}
        local pch = path.absolute(target:get("pcxxheader")):gsub("\\", "/"):lower()
        for _, flag in ipairs(compiler:compflags({target = target})) do
            local lower = flag:lower()
            if lower:match("^[/%-]fi") and lower:sub(4):gsub("\\", "/") ~= pch then
                cprint("${yellow}CommonLib cache: custom forced include; compiling locally")
                return
            end
            -- Diagnostics cannot change ABI/code. PCH output paths are per-project;
            -- its source is covered by the clean, pinned CommonLib revision.
            if not (lower:match("^[/%-]wd%d+") or lower:match("^[/%-]we%d+")
                or lower:match("^[/%-]w[0-4]$") or lower:match("^[/%-]external:w")
                or lower:match("^[/%-]d1trimfile:")
                or lower:match("^[/%-]yu") or lower:match("^[/%-]fp") and not lower:match("^[/%-]fp:")
                or lower:match("^[/%-]fi")) then
                flag = flag:gsub("\\", "/")
                flag = flag:replace(path.absolute(source):gsub("\\", "/"), "<COMMONLIB>", {plain = true})
                flag = flag:replace(path.relative(source, os.projectdir()):gsub("\\", "/"), "<COMMONLIB>", {plain = true})
                table.insert(flags, flag)
            end
        end
        local vc = target:toolchain("msvc"):config("vcvars") or {}
        local identity = {
            schema = 1, revision = revision, compiler = program,
            compiler_sha256 = hash.sha256(program),
            sdk = vc.WindowsSDKVersion or "", toolset = vc.VCToolsVersion or "",
            plat = config.get("plat"), arch = config.get("arch"), mode = config.get("mode"),
            flags = flags
        }
        local signature = table.concat({"commonlib-cache-v1", revision, program,
            identity.compiler_sha256, identity.sdk, identity.toolset,
            identity.plat, identity.arch, identity.mode, table.concat(flags, "\n")}, "\n")
        local key = hash.sha256(bytes(signature))
        local directory = path.join(get_config("commonlib_cache_dir"), revision, key)
        local metadata = path.join(directory, "manifest.json")
        local library = path.join(directory, "lib", "commonlibsse-ng.lib")
        local manifest = os.isfile(metadata) and json.loadfile(metadata) or nil
        target:data_set("shared_commonlib.identity", identity)
        target:data_set("shared_commonlib.directory", directory)
        target:data_set("shared_commonlib.key", key)
        if manifest then
            assert(manifest.key == key and os.isfile(library), "Incomplete CommonLib cache entry: " .. directory)
            -- Verify once per xmake process. The hash also detects accidental edits.
            assert(hash.sha256(library) == manifest.library_sha256, "Corrupt CommonLib cache entry: " .. directory)
            target:set("kind", "phony")
            target:set("pcxxheader", nil)
            target:set("files", nil)
            target:add("linkdirs", path.join(directory, "lib"), {public = true})
            target:add("links", "commonlibsse-ng", {public = true})
            target:add("syslinks", "advapi32", "bcrypt", "d3d11", "d3dcompiler", "dbghelp", "dxgi", "ole32", "shell32", "user32", "version", {public = true})
            target:data_set("shared_commonlib.hit", true)
            cprint("${green}CommonLib cache: HIT %s", key:sub(1, 16))
        else
            cprint("${yellow}CommonLib cache: MISS %s; building from source", key:sub(1, 16))
        end
    end)

    after_build(function(target)
        import("core.base.json")
        local directory = target:data("shared_commonlib.directory")
        if not directory or target:data("shared_commonlib.hit") then return end
        local identity = target:data("shared_commonlib.identity")
        local source = target:scriptdir()
        if os.iorunv("git", {"-C", source, "rev-parse", "HEAD"}):trim() ~= identity.revision
            or os.iorunv("git", {"-C", source, "status", "--porcelain", "--untracked-files=all"}):trim() ~= "" then
            cprint("${yellow}CommonLib changed during the build; not publishing to the shared cache")
            return
        end
        os.mkdir(path.directory(directory))
        local lock = assert(io.openlock(directory .. ".lock"))
        lock:lock()
        if os.isfile(path.join(directory, "manifest.json")) then
            lock:unlock()
            lock:close()
            return
        end
        -- Publish atomically. Concurrent producers never overwrite a completed entry.
        local staging = directory .. "." .. hash.uuid() .. ".tmp"
        os.mkdir(path.join(staging, "lib"))
        os.cp(target:targetfile(), path.join(staging, "lib", "commonlibsse-ng.lib"))
        if target:symbolfile() and os.isfile(target:symbolfile()) then
            os.cp(target:symbolfile(), path.join(staging, "lib", "commonlibsse-ng.pdb"))
        end
        local manifest = {
            key = target:data("shared_commonlib.key"),
            identity = identity,
            library_sha256 = hash.sha256(path.join(staging, "lib", "commonlibsse-ng.lib"))
        }
        json.savefile(path.join(staging, "manifest.json"), manifest)
        if not os.isdir(directory) then
            local ok = os.trymv(staging, directory)
            if not ok and not os.isfile(path.join(directory, "manifest.json")) then
                raise("Could not publish CommonLib cache: " .. directory)
            end
        end
        if os.isdir(staging) then os.rm(staging) end
        lock:unlock()
        lock:close()
        cprint("${green}CommonLib cache: published %s", manifest.key:sub(1, 16))
    end)
end)
