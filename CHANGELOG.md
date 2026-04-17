# Changelog

## 0.3.3-braze

- Fix Ruby 3 compatibility: `rb_profile_frames` can expose C frames without a
  file path ([Shopify/rotoscope#87](https://github.com/Shopify/rotoscope/issues/87)).
  Walk the profile stack until a Ruby frame with a path is found, fall back to
  `rb_sourcefile` when needed, and skip caller-path excludelist/includelist when
  the caller path is still empty so callgraphs stay complete for selective testing.
