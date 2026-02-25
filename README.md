# AssetCsvSync

**AssetCsvSync** is a metadata-driven synchronization plugin for Unreal Engine that enables bidirectional exchange between `UDataAsset` classes and CSV files.

The system uses reflection and lightweight `meta` tags to define how properties are mapped to CSV columns — without requiring custom base classes or code generation.

---

## Overview

AssetCsvSync provides:

- CSV → DataAsset import  
- DataAsset → CSV export   
- Fully reflection-based mapping

> ⚠️ This project is currently under active development.
> APIs and behavior may change without notice.
