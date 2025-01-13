-- vim: fdm=marker

return {

    {
        not_dependencies = {
            "lfs",
            "resvg",
            "rlwr",
        },
        artifact = "automation",
        main = "automation.c",
        src = "src",
        flags = {
            "-fopenmp",
        },
        --[[
        debug_define = { },
        release_define = { },
        --]]
    },

}
