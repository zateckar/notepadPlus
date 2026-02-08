# PowerShell script to create a simple Notepad+ icon
# This creates a 256x256 icon with "N+" text

Add-Type -AssemblyName System.Drawing

# Function to create icon at specified size
function Create-IconBitmap {
    param(
        [int]$size
    )
    
    $bitmap = New-Object System.Drawing.Bitmap($size, $size)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
    
    # Background gradient (blue theme)
    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $color1 = [System.Drawing.Color]::FromArgb(255, 41, 128, 185)  # Blue
    $color2 = [System.Drawing.Color]::FromArgb(255, 52, 152, 219)  # Light blue
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $color1, $color2, 45)
    $graphics.FillRectangle($brush, $rect)
    
    # Add border
    $borderPen = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(255, 30, 96, 139), [Math]::Max(2, $size / 64))
    $graphics.DrawRectangle($borderPen, 0, 0, $size - 1, $size - 1)
    
    # Draw "N+" text
    $fontSize = [Math]::Max(12, $size * 0.4)
    $font = New-Object System.Drawing.Font("Arial", $fontSize, [System.Drawing.FontStyle]::Bold)
    $textBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    
    $text = "N+"
    $textSize = $graphics.MeasureString($text, $font)
    $x = ($size - $textSize.Width) / 2
    $y = ($size - $textSize.Height) / 2
    
    # Draw shadow
    $shadowBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(128, 0, 0, 0))
    $shadowOffset = [Math]::Max(1, $size / 64)
    $graphics.DrawString($text, $font, $shadowBrush, $x + $shadowOffset, $y + $shadowOffset)
    
    # Draw text
    $graphics.DrawString($text, $font, $textBrush, $x, $y)
    
    $graphics.Dispose()
    $brush.Dispose()
    $borderPen.Dispose()
    $font.Dispose()
    $textBrush.Dispose()
    $shadowBrush.Dispose()
    
    return $bitmap
}

# Function to save as ICO file with multiple sizes
function Save-Icon {
    param(
        [string]$path
    )
    
    Write-Host "Creating icon file: $path"
    
    # Create bitmaps for different sizes
    $sizes = @(256, 128, 64, 48, 32, 16)
    $bitmaps = @()
    
    foreach ($size in $sizes) {
        Write-Host "  Creating ${size}x${size} bitmap..."
        $bitmaps += Create-IconBitmap -size $size
    }
    
    # Save as ICO file using .NET Icon class
    $icon256 = $bitmaps[0]
    $ms = New-Object System.IO.MemoryStream
    $icon256.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $ms.Seek(0, [System.IO.SeekOrigin]::Begin) | Out-Null
    
    # Create icon from bitmap
    $iconHandle = $icon256.GetHicon()
    $icon = [System.Drawing.Icon]::FromHandle($iconHandle)
    
    # Save icon
    $fs = New-Object System.IO.FileStream($path, [System.IO.FileMode]::Create)
    $icon.Save($fs)
    $fs.Close()
    
    # Cleanup
    foreach ($bmp in $bitmaps) {
        $bmp.Dispose()
    }
    $icon.Dispose()
    $ms.Dispose()
    
    Write-Host "Icon created successfully!"
}

# Main execution
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$iconPath = Join-Path $scriptDir "notepad+.ico"

try {
    Save-Icon -path $iconPath
    Write-Host "`nIcon file created at: $iconPath"
    Write-Host "You can now build the application with the icon included."
}
catch {
    Write-Host "Error creating icon: $_"
    Write-Host "`nAlternatively, you can create a custom .ico file using an icon editor"
    Write-Host "and save it as: $iconPath"
    exit 1
}