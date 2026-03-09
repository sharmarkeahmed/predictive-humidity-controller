[CmdletBinding()]
param(
    [switch]$SkipGeneratedBuildFiles
)

$ErrorActionPreference = 'Stop'

function Get-ProjectName {
    param(
        [string]$ProjectFile,
        [string]$Fallback
    )

    if (Test-Path $ProjectFile) {
        [xml]$projectXml = Get-Content -Path $ProjectFile
        if ($projectXml.projectDescription.name) {
            return $projectXml.projectDescription.name
        }
    }

    return $Fallback
}

function Save-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function Ensure-IncludePath {
    param(
        [System.Xml.XmlDocument]$XmlDoc,
        [string]$OptionIdPattern,
        [string]$IncludeValue
    )

    $options = $XmlDoc.SelectNodes("//option[contains(@superClass,'$OptionIdPattern')]")
    foreach ($option in $options) {
        $existing = @($option.SelectNodes("listOptionValue[@value='$IncludeValue']"))
        if ($existing.Count -eq 0) {
            $node = $XmlDoc.CreateElement("listOptionValue")
            $null = $node.SetAttribute("builtIn", "false")
            $null = $node.SetAttribute("value", $IncludeValue)
            $null = $option.AppendChild($node)
        }
    }
}

function Set-SourceEntries {
    param(
        [System.Xml.XmlDocument]$XmlDoc
    )

    $configs = $XmlDoc.SelectNodes("//cconfiguration/storageModule[@moduleId='cdtBuildSystem']/configuration")
    foreach ($config in $configs) {
        $sourceEntries = $config.SelectSingleNode("sourceEntries")
        if (-not $sourceEntries) {
            continue
        }

        while ($sourceEntries.HasChildNodes) {
            $null = $sourceEntries.RemoveChild($sourceEntries.FirstChild)
        }

        $entries = @(
            @{ name = "App"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath" },
            @{ name = "Core"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath" },
            @{ name = "Drivers"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath"; excluding = "CMSIS|STM32F4xx_HAL_Driver" },
            @{ name = "Drivers/CMSIS/Device/ST/STM32F4xx/Source/Templates"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath" },
            @{ name = "Drivers/STM32F4xx_HAL_Driver"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath" },
            @{ name = "Middlewares"; flags = "VALUE_WORKSPACE_PATH|RESOLVED"; kind = "sourcePath" }
        )

        foreach ($entryDef in $entries) {
            $entry = $XmlDoc.CreateElement("entry")
            $null = $entry.SetAttribute("flags", $entryDef.flags)
            $null = $entry.SetAttribute("kind", $entryDef.kind)
            $null = $entry.SetAttribute("name", $entryDef.name)
            if ($entryDef.ContainsKey("excluding")) {
                $null = $entry.SetAttribute("excluding", $entryDef.excluding)
            }
            $null = $sourceEntries.AppendChild($entry)
        }
    }
}

function Update-CProject {
    param(
        [string]$Path,
        [string]$ProjectName
    )

    [xml]$xml = Get-Content -Path $Path

    Ensure-IncludePath -XmlDoc $xml -OptionIdPattern "tool.assembler.option.includepaths" -IncludeValue "../Drivers/CMSIS/RTOS2/Include"
    Ensure-IncludePath -XmlDoc $xml -OptionIdPattern "tool.c.compiler.option.includepaths" -IncludeValue "../Drivers/CMSIS/RTOS2/Include"
    Set-SourceEntries -XmlDoc $xml

    $projectNode = $xml.SelectSingleNode("//storageModule[@moduleId='cdtBuildSystem']/project")
    if ($projectNode) {
        $null = $projectNode.SetAttribute("name", $ProjectName)
        $id = $projectNode.GetAttribute("id")
        if ($id -match '^[^.]+(\..+)$') {
            $null = $projectNode.SetAttribute("id", "$ProjectName$($Matches[1])")
        }
    }

    $settings = New-Object System.Xml.XmlWriterSettings
    $settings.Indent = $true
    $settings.IndentChars = "`t"
    $settings.NewLineChars = "`r`n"
    $settings.NewLineHandling = [System.Xml.NewLineHandling]::Replace
    $settings.Encoding = New-Object System.Text.UTF8Encoding($false)

    $writer = [System.Xml.XmlWriter]::Create($Path, $settings)
    try {
        $xml.Save($writer)
    }
    finally {
        $writer.Dispose()
    }
}

function Add-HeaderPathToken {
    param(
        [string]$Line,
        [string]$Token
    )

    if ($Line -notmatch '^HeaderPath=') {
        return $Line
    }

    $prefix, $values = $Line -split '=', 2
    $parts = @($values -split ';' | Where-Object { $_ -ne '' })
    if ($parts -notcontains $Token) {
        $insertAfter = [Array]::IndexOf($parts, ($parts | Where-Object { $_ -like '*CMSIS_RTOS_V2' } | Select-Object -First 1))
        if ($insertAfter -ge 0) {
            $parts = @($parts[0..$insertAfter] + $Token + $parts[($insertAfter + 1)..($parts.Length - 1)])
        }
        else {
            $parts += $Token
        }
    }

    return "$prefix=$($parts -join ';');"
}

function Update-MxProject {
    param(
        [string]$Path
    )

    $lines = Get-Content -Path $Path
    $updated = foreach ($line in $lines) {
        if ($line -like 'HeaderPath=*') {
            if ($line -like '..\*') {
                Add-HeaderPathToken -Line $line -Token '..\Drivers\CMSIS\RTOS2\Include'
            }
            else {
                Add-HeaderPathToken -Line $line -Token 'Drivers\CMSIS\RTOS2\Include'
            }
        }
        else {
            $line
        }
    }

    Save-Utf8NoBom -Path $Path -Content (($updated -join "`r`n") + "`r`n")
}

function Get-LinkerScriptName {
    param(
        [string]$Root
    )

    $script = Get-ChildItem -Path $Root -Filter '*FLASH.ld' | Select-Object -First 1
    if (-not $script) {
        throw "No FLASH linker script found in $Root"
    }

    return $script.Name
}

function New-SourcesMkContent {
    @"
################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

ELF_SRCS :=
OBJ_SRCS :=
S_SRCS :=
C_SRCS :=
S_UPPER_SRCS :=
O_SRCS :=
CYCLO_FILES :=
SIZE_OUTPUT :=
OBJDUMP_LIST :=
SU_FILES :=
EXECUTABLES :=
OBJS :=
MAP_FILES :=
S_DEPS :=
S_UPPER_DEPS :=
C_DEPS :=

# Every subdirectory with source files must be described here
SUBDIRS := \
App/sensors \
Core/Src \
Core/Startup \
Drivers/STM32F4xx_HAL_Driver/Src \
Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 \
Middlewares/Third_Party/FreeRTOS/Source \
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang \
"@
}

function New-MakefileContent {
    param(
        [string]$ProjectName,
        [string]$LinkerScriptName
    )

    @"
################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (14.3.rel1)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/subdir.mk
-include Middlewares/Third_Party/FreeRTOS/Source/subdir.mk
-include Drivers/STM32F4xx_HAL_Driver/Src/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include App/sensors/subdir.mk
-include objects.mk

ifneq (`$(MAKECMDGOALS),clean)
ifneq (`$(strip `$\(S_DEPS)),)
-include `$\(S_DEPS)
endif
ifneq (`$(strip `$\(S_UPPER_DEPS)),)
-include `$\(S_UPPER_DEPS)
endif
ifneq (`$(strip `$\(C_DEPS)),)
-include `$\(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
`$(wildcard ../makefile.defs) \
`$(wildcard ../makefile.init) \
`$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := $ProjectName
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := `$\(BUILD_ARTIFACT_PREFIX)`$\(BUILD_ARTIFACT_NAME)`$(if `$\(BUILD_ARTIFACT_EXTENSION),.`$\(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables
EXECUTABLES += \
$ProjectName.elf \

MAP_FILES += \
$ProjectName.map \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
$ProjectName.list \


# All Target
all: main-build

# Main-build Target
main-build: $ProjectName.elf secondary-outputs

# Tool invocations
$ProjectName.elf $ProjectName.map: `$\(OBJS) `$\(USER_OBJS) $((Resolve-Path .).Path)\$LinkerScriptName makefile objects.list `$\(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "$ProjectName.elf" @"objects.list" `$\(USER_OBJS) `$\(LIBS) -mcpu=cortex-m4 -T"$((Resolve-Path .).Path)\$LinkerScriptName" --specs=nosys.specs -Wl,-Map="$ProjectName.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: `$\(EXECUTABLES) makefile objects.list `$\(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  `$\(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

$ProjectName.list: `$\(EXECUTABLES) makefile objects.list `$\(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S `$\(EXECUTABLES) > "$ProjectName.list"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-`$\(RM) default.size.stdout $ProjectName.elf $ProjectName.list $ProjectName.map
	-@echo ' '

secondary-outputs: `$\(SIZE_OUTPUT) `$\(OBJDUMP_LIST)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
"@
}

function Update-GeneratedBuildFiles {
    param(
        [string]$Root,
        [string]$ProjectName
    )

    $linkerScriptName = Get-LinkerScriptName -Root $Root
    $sourcesMk = New-SourcesMkContent

    foreach ($config in @('Debug', 'Release')) {
        $configPath = Join-Path $Root $config
        if (-not (Test-Path $configPath)) {
            continue
        }

        Save-Utf8NoBom -Path (Join-Path $configPath 'sources.mk') -Content ($sourcesMk.TrimStart("`r", "`n") + "`r`n")
        Save-Utf8NoBom -Path (Join-Path $configPath 'makefile') -Content ((New-MakefileContent -ProjectName $ProjectName -LinkerScriptName $linkerScriptName).TrimStart("`r", "`n") + "`r`n")
    }
}

$root = $PSScriptRoot
$projectName = Get-ProjectName -ProjectFile (Join-Path $root '.project') -Fallback (Split-Path $root -Leaf)

Update-CProject -Path (Join-Path $root '.cproject') -ProjectName $projectName
Update-MxProject -Path (Join-Path $root '.mxproject')

if (-not $SkipGeneratedBuildFiles) {
    Update-GeneratedBuildFiles -Root $root -ProjectName $projectName
}

Write-Host "Patched CubeMX project metadata for '$projectName'."
if ($SkipGeneratedBuildFiles) {
    Write-Host "Skipped Debug/Release generated build files."
}
else {
    Write-Host "Updated Debug/Release generated build files."
}
