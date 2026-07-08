#pragma once

namespace FirmwareInfo {

// Single source of truth for firmware identity.
static constexpr char kVersion[] = "v0.16.5";
static constexpr char kReleaseLabel[] = "raw-fifo-stable-ui";
static constexpr char kBuildDate[] = __DATE__;
static constexpr char kBuildTime[] = __TIME__;
static constexpr char kBuildStamp[] = __DATE__ " " __TIME__;

}  // namespace FirmwareInfo
