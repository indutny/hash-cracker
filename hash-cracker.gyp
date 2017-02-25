{
  "variables": {
    "thread_count%": 8
  },
  "target_defaults": {
    "include_dirs": [
      ".",
    ],

    "libraries": [
      "-lpthread",
      "-lm",
    ],
  },
  "targets": [{
    "target_name": "brute-gpu",
    "type": "executable",

    "include_dirs": [
      "<(SHARED_INTERMEDIATE_DIR)",
    ],

    "sources": [
      "src/brute-gpu.c",
    ],

    "conditions": [
      ["OS=='mac'", {
        "libraries": [
          "-framework OpenCL",
        ],
      }, {
        "libraries": [
          "-lOpenCL",
        ],
      }],
    ],

    "actions": [{
      'action_name': 'source2blob',
      'inputs': [
        'src/brute-gpu-program.cl'
      ],
      'outputs': [
        '<(SHARED_INTERMEDIATE_DIR)/src/brute-gpu-program.h'
      ],
      'action': [
        'node',
        'tools/source2blob.js',
        'brute-gpu-program',
        '<@(_inputs)',
        '<@(_outputs)',
      ],
    }],
  }, {
    "target_name": "client",
    "type": "executable",

    "sources": [
      "src/client.c",
    ],
  }],
}
