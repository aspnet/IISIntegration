<#
.DESCRIPTION
Sets redirection.config enabled flag to false. 
Requires admin privileges.
#>
[cmdletbinding(SupportsShouldProcess = $true)]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 1

$redirectionConfigFile = Resolve-Path "${env:windir}\system32\inetsrv\config\redirection.config";

[bool]$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin -and -not $WhatIfPreference) {
    if ($PSCmdlet.ShouldContinue("Continue as an admin?", "This script needs admin privileges to update IIS Express and IIS.")) {
        $thisFile = Join-Path $PSScriptRoot $MyInvocation.MyCommand.Name

        Start-Process `
            -Verb runas `
            -FilePath "powershell.exe" `
            -ArgumentList $thisFile `
            -Wait `
            | Out-Null

        if (-not $?) {
            throw 'Update failed'
        }
        exit
    }
    else {
        throw 'Requires admin privileges'
    }
}

$content = [System.IO.File]::ReadAllText($redirectionConfigFile).Replace('enabled="true"','enabled="false"')
[System.IO.File]::WriteAllText($redirectionConfigFile, $content)