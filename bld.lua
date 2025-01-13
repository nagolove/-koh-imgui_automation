

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




   },

}
