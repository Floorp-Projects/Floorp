Index-Image
===========

This image is designed to be used for indexing other tasks. It takes a task
definition as follows:
```js
{
  ...,
  scopes: [
    'index:insert-task:my-index.namespace',
    'index:insert-task:...',
  ],
  payload: {
    image: '...',
    env: {
      TARGET_TASKID: '<taskId-to-be-indexed>',
    },
    command: [
      'insert-indexes.js',
      'my-index.namespace.one',
      'my-index.namespace.two',
      '....',
    ],
    features: {
      taskclusterProxy: true,
    },
    maxRunTime: 600,
  },
}
```

As can be seen the `taskId` to be indexed is given by the environment variable
`TARGET_TASKID` and the `command` arguments specifies namespaces that it must
be index under. It is **important** to also include scopes on the form
`index:insert-task:<...>` for all namespaces `<...>` given as `command`
arguments.
