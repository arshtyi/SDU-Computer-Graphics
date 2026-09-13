# test

```sh
xmake test
xmake test PointLocation/basic
xmake run PointLocation tests/basic.in
xmake run PointLocation < tests/basic.in
```

| case         | covers                                     |
| ------------ | ------------------------------------------ |
| `basic`      | 内部、水平边、斜边、顶点、外部及边的延长线 |
| `clockwise`  | 调换顶点顺序，预期结果与 basic 相同        |
| `vertex_ray` | 射线经过中间高度顶点、最低点和最高点       |
| `collinear`  | 共线顶点，预期非零退出码                   |
| `repeated`   | 重复顶点，预期非零退出码                   |
