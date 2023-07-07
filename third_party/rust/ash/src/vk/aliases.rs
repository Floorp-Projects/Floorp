use crate::vk::bitflags::*;
use crate::vk::definitions::*;
use crate::vk::enums::*;
pub type GeometryFlagsNV = GeometryFlagsKHR;
pub type GeometryInstanceFlagsNV = GeometryInstanceFlagsKHR;
pub type BuildAccelerationStructureFlagsNV = BuildAccelerationStructureFlagsKHR;
pub type PrivateDataSlotCreateFlagsEXT = PrivateDataSlotCreateFlags;
pub type DescriptorUpdateTemplateCreateFlagsKHR = DescriptorUpdateTemplateCreateFlags;
pub type PipelineCreationFeedbackFlagsEXT = PipelineCreationFeedbackFlags;
pub type SemaphoreWaitFlagsKHR = SemaphoreWaitFlags;
pub type AccessFlags2KHR = AccessFlags2;
pub type PipelineStageFlags2KHR = PipelineStageFlags2;
pub type FormatFeatureFlags2KHR = FormatFeatureFlags2;
pub type RenderingFlagsKHR = RenderingFlags;
pub type PeerMemoryFeatureFlagsKHR = PeerMemoryFeatureFlags;
pub type MemoryAllocateFlagsKHR = MemoryAllocateFlags;
pub type CommandPoolTrimFlagsKHR = CommandPoolTrimFlags;
pub type ExternalMemoryHandleTypeFlagsKHR = ExternalMemoryHandleTypeFlags;
pub type ExternalMemoryFeatureFlagsKHR = ExternalMemoryFeatureFlags;
pub type ExternalSemaphoreHandleTypeFlagsKHR = ExternalSemaphoreHandleTypeFlags;
pub type ExternalSemaphoreFeatureFlagsKHR = ExternalSemaphoreFeatureFlags;
pub type SemaphoreImportFlagsKHR = SemaphoreImportFlags;
pub type ExternalFenceHandleTypeFlagsKHR = ExternalFenceHandleTypeFlags;
pub type ExternalFenceFeatureFlagsKHR = ExternalFenceFeatureFlags;
pub type FenceImportFlagsKHR = FenceImportFlags;
pub type DescriptorBindingFlagsEXT = DescriptorBindingFlags;
pub type ResolveModeFlagsKHR = ResolveModeFlags;
pub type ToolPurposeFlagsEXT = ToolPurposeFlags;
pub type SubmitFlagsKHR = SubmitFlags;
pub type DescriptorUpdateTemplateKHR = DescriptorUpdateTemplate;
pub type SamplerYcbcrConversionKHR = SamplerYcbcrConversion;
pub type PrivateDataSlotEXT = PrivateDataSlot;
pub type DescriptorUpdateTemplateTypeKHR = DescriptorUpdateTemplateType;
pub type PointClippingBehaviorKHR = PointClippingBehavior;
pub type QueueGlobalPriorityEXT = QueueGlobalPriorityKHR;
pub type SemaphoreTypeKHR = SemaphoreType;
pub type CopyAccelerationStructureModeNV = CopyAccelerationStructureModeKHR;
pub type AccelerationStructureTypeNV = AccelerationStructureTypeKHR;
pub type GeometryTypeNV = GeometryTypeKHR;
pub type RayTracingShaderGroupTypeNV = RayTracingShaderGroupTypeKHR;
pub type TessellationDomainOriginKHR = TessellationDomainOrigin;
pub type SamplerYcbcrModelConversionKHR = SamplerYcbcrModelConversion;
pub type SamplerYcbcrRangeKHR = SamplerYcbcrRange;
pub type ChromaLocationKHR = ChromaLocation;
pub type SamplerReductionModeEXT = SamplerReductionMode;
pub type ShaderFloatControlsIndependenceKHR = ShaderFloatControlsIndependence;
pub type DriverIdKHR = DriverId;
pub type DevicePrivateDataCreateInfoEXT = DevicePrivateDataCreateInfo;
pub type PrivateDataSlotCreateInfoEXT = PrivateDataSlotCreateInfo;
pub type PhysicalDevicePrivateDataFeaturesEXT = PhysicalDevicePrivateDataFeatures;
pub type PhysicalDeviceFeatures2KHR = PhysicalDeviceFeatures2;
pub type PhysicalDeviceProperties2KHR = PhysicalDeviceProperties2;
pub type FormatProperties2KHR = FormatProperties2;
pub type ImageFormatProperties2KHR = ImageFormatProperties2;
pub type PhysicalDeviceImageFormatInfo2KHR = PhysicalDeviceImageFormatInfo2;
pub type QueueFamilyProperties2KHR = QueueFamilyProperties2;
pub type PhysicalDeviceMemoryProperties2KHR = PhysicalDeviceMemoryProperties2;
pub type SparseImageFormatProperties2KHR = SparseImageFormatProperties2;
pub type PhysicalDeviceSparseImageFormatInfo2KHR = PhysicalDeviceSparseImageFormatInfo2;
pub type ConformanceVersionKHR = ConformanceVersion;
pub type PhysicalDeviceDriverPropertiesKHR = PhysicalDeviceDriverProperties;
pub type PhysicalDeviceVariablePointersFeaturesKHR = PhysicalDeviceVariablePointersFeatures;
pub type PhysicalDeviceVariablePointerFeaturesKHR = PhysicalDeviceVariablePointersFeatures;
pub type PhysicalDeviceVariablePointerFeatures = PhysicalDeviceVariablePointersFeatures;
pub type ExternalMemoryPropertiesKHR = ExternalMemoryProperties;
pub type PhysicalDeviceExternalImageFormatInfoKHR = PhysicalDeviceExternalImageFormatInfo;
pub type ExternalImageFormatPropertiesKHR = ExternalImageFormatProperties;
pub type PhysicalDeviceExternalBufferInfoKHR = PhysicalDeviceExternalBufferInfo;
pub type ExternalBufferPropertiesKHR = ExternalBufferProperties;
pub type PhysicalDeviceIDPropertiesKHR = PhysicalDeviceIDProperties;
pub type ExternalMemoryImageCreateInfoKHR = ExternalMemoryImageCreateInfo;
pub type ExternalMemoryBufferCreateInfoKHR = ExternalMemoryBufferCreateInfo;
pub type ExportMemoryAllocateInfoKHR = ExportMemoryAllocateInfo;
pub type PhysicalDeviceExternalSemaphoreInfoKHR = PhysicalDeviceExternalSemaphoreInfo;
pub type ExternalSemaphorePropertiesKHR = ExternalSemaphoreProperties;
pub type ExportSemaphoreCreateInfoKHR = ExportSemaphoreCreateInfo;
pub type PhysicalDeviceExternalFenceInfoKHR = PhysicalDeviceExternalFenceInfo;
pub type ExternalFencePropertiesKHR = ExternalFenceProperties;
pub type ExportFenceCreateInfoKHR = ExportFenceCreateInfo;
pub type PhysicalDeviceMultiviewFeaturesKHR = PhysicalDeviceMultiviewFeatures;
pub type PhysicalDeviceMultiviewPropertiesKHR = PhysicalDeviceMultiviewProperties;
pub type RenderPassMultiviewCreateInfoKHR = RenderPassMultiviewCreateInfo;
pub type PhysicalDeviceGroupPropertiesKHR = PhysicalDeviceGroupProperties;
pub type MemoryAllocateFlagsInfoKHR = MemoryAllocateFlagsInfo;
pub type BindBufferMemoryInfoKHR = BindBufferMemoryInfo;
pub type BindBufferMemoryDeviceGroupInfoKHR = BindBufferMemoryDeviceGroupInfo;
pub type BindImageMemoryInfoKHR = BindImageMemoryInfo;
pub type BindImageMemoryDeviceGroupInfoKHR = BindImageMemoryDeviceGroupInfo;
pub type DeviceGroupRenderPassBeginInfoKHR = DeviceGroupRenderPassBeginInfo;
pub type DeviceGroupCommandBufferBeginInfoKHR = DeviceGroupCommandBufferBeginInfo;
pub type DeviceGroupSubmitInfoKHR = DeviceGroupSubmitInfo;
pub type DeviceGroupBindSparseInfoKHR = DeviceGroupBindSparseInfo;
pub type DeviceGroupDeviceCreateInfoKHR = DeviceGroupDeviceCreateInfo;
pub type DescriptorUpdateTemplateEntryKHR = DescriptorUpdateTemplateEntry;
pub type DescriptorUpdateTemplateCreateInfoKHR = DescriptorUpdateTemplateCreateInfo;
pub type InputAttachmentAspectReferenceKHR = InputAttachmentAspectReference;
pub type RenderPassInputAttachmentAspectCreateInfoKHR = RenderPassInputAttachmentAspectCreateInfo;
pub type PhysicalDevice16BitStorageFeaturesKHR = PhysicalDevice16BitStorageFeatures;
pub type PhysicalDeviceShaderSubgroupExtendedTypesFeaturesKHR =
    PhysicalDeviceShaderSubgroupExtendedTypesFeatures;
pub type BufferMemoryRequirementsInfo2KHR = BufferMemoryRequirementsInfo2;
pub type DeviceBufferMemoryRequirementsKHR = DeviceBufferMemoryRequirements;
pub type ImageMemoryRequirementsInfo2KHR = ImageMemoryRequirementsInfo2;
pub type ImageSparseMemoryRequirementsInfo2KHR = ImageSparseMemoryRequirementsInfo2;
pub type DeviceImageMemoryRequirementsKHR = DeviceImageMemoryRequirements;
pub type MemoryRequirements2KHR = MemoryRequirements2;
pub type SparseImageMemoryRequirements2KHR = SparseImageMemoryRequirements2;
pub type PhysicalDevicePointClippingPropertiesKHR = PhysicalDevicePointClippingProperties;
pub type MemoryDedicatedRequirementsKHR = MemoryDedicatedRequirements;
pub type MemoryDedicatedAllocateInfoKHR = MemoryDedicatedAllocateInfo;
pub type ImageViewUsageCreateInfoKHR = ImageViewUsageCreateInfo;
pub type PipelineTessellationDomainOriginStateCreateInfoKHR =
    PipelineTessellationDomainOriginStateCreateInfo;
pub type SamplerYcbcrConversionInfoKHR = SamplerYcbcrConversionInfo;
pub type SamplerYcbcrConversionCreateInfoKHR = SamplerYcbcrConversionCreateInfo;
pub type BindImagePlaneMemoryInfoKHR = BindImagePlaneMemoryInfo;
pub type ImagePlaneMemoryRequirementsInfoKHR = ImagePlaneMemoryRequirementsInfo;
pub type PhysicalDeviceSamplerYcbcrConversionFeaturesKHR =
    PhysicalDeviceSamplerYcbcrConversionFeatures;
pub type SamplerYcbcrConversionImageFormatPropertiesKHR =
    SamplerYcbcrConversionImageFormatProperties;
pub type PhysicalDeviceSamplerFilterMinmaxPropertiesEXT =
    PhysicalDeviceSamplerFilterMinmaxProperties;
pub type SamplerReductionModeCreateInfoEXT = SamplerReductionModeCreateInfo;
pub type PhysicalDeviceInlineUniformBlockFeaturesEXT = PhysicalDeviceInlineUniformBlockFeatures;
pub type PhysicalDeviceInlineUniformBlockPropertiesEXT = PhysicalDeviceInlineUniformBlockProperties;
pub type WriteDescriptorSetInlineUniformBlockEXT = WriteDescriptorSetInlineUniformBlock;
pub type DescriptorPoolInlineUniformBlockCreateInfoEXT = DescriptorPoolInlineUniformBlockCreateInfo;
pub type ImageFormatListCreateInfoKHR = ImageFormatListCreateInfo;
pub type PhysicalDeviceMaintenance3PropertiesKHR = PhysicalDeviceMaintenance3Properties;
pub type PhysicalDeviceMaintenance4FeaturesKHR = PhysicalDeviceMaintenance4Features;
pub type PhysicalDeviceMaintenance4PropertiesKHR = PhysicalDeviceMaintenance4Properties;
pub type DescriptorSetLayoutSupportKHR = DescriptorSetLayoutSupport;
pub type PhysicalDeviceShaderDrawParameterFeatures = PhysicalDeviceShaderDrawParametersFeatures;
pub type PhysicalDeviceShaderFloat16Int8FeaturesKHR = PhysicalDeviceShaderFloat16Int8Features;
pub type PhysicalDeviceFloat16Int8FeaturesKHR = PhysicalDeviceShaderFloat16Int8Features;
pub type PhysicalDeviceFloatControlsPropertiesKHR = PhysicalDeviceFloatControlsProperties;
pub type PhysicalDeviceHostQueryResetFeaturesEXT = PhysicalDeviceHostQueryResetFeatures;
pub type DeviceQueueGlobalPriorityCreateInfoEXT = DeviceQueueGlobalPriorityCreateInfoKHR;
pub type PhysicalDeviceGlobalPriorityQueryFeaturesEXT =
    PhysicalDeviceGlobalPriorityQueryFeaturesKHR;
pub type QueueFamilyGlobalPriorityPropertiesEXT = QueueFamilyGlobalPriorityPropertiesKHR;
pub type PhysicalDeviceDescriptorIndexingFeaturesEXT = PhysicalDeviceDescriptorIndexingFeatures;
pub type PhysicalDeviceDescriptorIndexingPropertiesEXT = PhysicalDeviceDescriptorIndexingProperties;
pub type DescriptorSetLayoutBindingFlagsCreateInfoEXT = DescriptorSetLayoutBindingFlagsCreateInfo;
pub type DescriptorSetVariableDescriptorCountAllocateInfoEXT =
    DescriptorSetVariableDescriptorCountAllocateInfo;
pub type DescriptorSetVariableDescriptorCountLayoutSupportEXT =
    DescriptorSetVariableDescriptorCountLayoutSupport;
pub type AttachmentDescription2KHR = AttachmentDescription2;
pub type AttachmentReference2KHR = AttachmentReference2;
pub type SubpassDescription2KHR = SubpassDescription2;
pub type SubpassDependency2KHR = SubpassDependency2;
pub type RenderPassCreateInfo2KHR = RenderPassCreateInfo2;
pub type SubpassBeginInfoKHR = SubpassBeginInfo;
pub type SubpassEndInfoKHR = SubpassEndInfo;
pub type PhysicalDeviceTimelineSemaphoreFeaturesKHR = PhysicalDeviceTimelineSemaphoreFeatures;
pub type PhysicalDeviceTimelineSemaphorePropertiesKHR = PhysicalDeviceTimelineSemaphoreProperties;
pub type SemaphoreTypeCreateInfoKHR = SemaphoreTypeCreateInfo;
pub type TimelineSemaphoreSubmitInfoKHR = TimelineSemaphoreSubmitInfo;
pub type SemaphoreWaitInfoKHR = SemaphoreWaitInfo;
pub type SemaphoreSignalInfoKHR = SemaphoreSignalInfo;
pub type PhysicalDevice8BitStorageFeaturesKHR = PhysicalDevice8BitStorageFeatures;
pub type PhysicalDeviceVulkanMemoryModelFeaturesKHR = PhysicalDeviceVulkanMemoryModelFeatures;
pub type PhysicalDeviceShaderAtomicInt64FeaturesKHR = PhysicalDeviceShaderAtomicInt64Features;
pub type PhysicalDeviceDepthStencilResolvePropertiesKHR =
    PhysicalDeviceDepthStencilResolveProperties;
pub type SubpassDescriptionDepthStencilResolveKHR = SubpassDescriptionDepthStencilResolve;
pub type PhysicalDeviceFragmentShaderBarycentricFeaturesNV =
    PhysicalDeviceFragmentShaderBarycentricFeaturesKHR;
pub type ImageStencilUsageCreateInfoEXT = ImageStencilUsageCreateInfo;
pub type PhysicalDeviceScalarBlockLayoutFeaturesEXT = PhysicalDeviceScalarBlockLayoutFeatures;
pub type PhysicalDeviceUniformBufferStandardLayoutFeaturesKHR =
    PhysicalDeviceUniformBufferStandardLayoutFeatures;
pub type PhysicalDeviceBufferDeviceAddressFeaturesKHR = PhysicalDeviceBufferDeviceAddressFeatures;
pub type PhysicalDeviceBufferAddressFeaturesEXT = PhysicalDeviceBufferDeviceAddressFeaturesEXT;
pub type BufferDeviceAddressInfoKHR = BufferDeviceAddressInfo;
pub type BufferDeviceAddressInfoEXT = BufferDeviceAddressInfo;
pub type BufferOpaqueCaptureAddressCreateInfoKHR = BufferOpaqueCaptureAddressCreateInfo;
pub type PhysicalDeviceImagelessFramebufferFeaturesKHR = PhysicalDeviceImagelessFramebufferFeatures;
pub type FramebufferAttachmentsCreateInfoKHR = FramebufferAttachmentsCreateInfo;
pub type FramebufferAttachmentImageInfoKHR = FramebufferAttachmentImageInfo;
pub type RenderPassAttachmentBeginInfoKHR = RenderPassAttachmentBeginInfo;
pub type PhysicalDeviceTextureCompressionASTCHDRFeaturesEXT =
    PhysicalDeviceTextureCompressionASTCHDRFeatures;
pub type PipelineCreationFeedbackEXT = PipelineCreationFeedback;
pub type PipelineCreationFeedbackCreateInfoEXT = PipelineCreationFeedbackCreateInfo;
pub type QueryPoolCreateInfoINTEL = QueryPoolPerformanceQueryCreateInfoINTEL;
pub type PhysicalDeviceSeparateDepthStencilLayoutsFeaturesKHR =
    PhysicalDeviceSeparateDepthStencilLayoutsFeatures;
pub type AttachmentReferenceStencilLayoutKHR = AttachmentReferenceStencilLayout;
pub type AttachmentDescriptionStencilLayoutKHR = AttachmentDescriptionStencilLayout;
pub type PipelineInfoEXT = PipelineInfoKHR;
pub type PhysicalDeviceShaderDemoteToHelperInvocationFeaturesEXT =
    PhysicalDeviceShaderDemoteToHelperInvocationFeatures;
pub type PhysicalDeviceTexelBufferAlignmentPropertiesEXT =
    PhysicalDeviceTexelBufferAlignmentProperties;
pub type PhysicalDeviceSubgroupSizeControlFeaturesEXT = PhysicalDeviceSubgroupSizeControlFeatures;
pub type PhysicalDeviceSubgroupSizeControlPropertiesEXT =
    PhysicalDeviceSubgroupSizeControlProperties;
pub type PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT =
    PipelineShaderStageRequiredSubgroupSizeCreateInfo;
pub type ShaderRequiredSubgroupSizeCreateInfoEXT =
    PipelineShaderStageRequiredSubgroupSizeCreateInfo;
pub type MemoryOpaqueCaptureAddressAllocateInfoKHR = MemoryOpaqueCaptureAddressAllocateInfo;
pub type DeviceMemoryOpaqueCaptureAddressInfoKHR = DeviceMemoryOpaqueCaptureAddressInfo;
pub type PhysicalDevicePipelineCreationCacheControlFeaturesEXT =
    PhysicalDevicePipelineCreationCacheControlFeatures;
pub type PhysicalDeviceToolPropertiesEXT = PhysicalDeviceToolProperties;
pub type AabbPositionsNV = AabbPositionsKHR;
pub type TransformMatrixNV = TransformMatrixKHR;
pub type AccelerationStructureInstanceNV = AccelerationStructureInstanceKHR;
pub type PhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR =
    PhysicalDeviceZeroInitializeWorkgroupMemoryFeatures;
pub type PhysicalDeviceImageRobustnessFeaturesEXT = PhysicalDeviceImageRobustnessFeatures;
pub type BufferCopy2KHR = BufferCopy2;
pub type ImageCopy2KHR = ImageCopy2;
pub type ImageBlit2KHR = ImageBlit2;
pub type BufferImageCopy2KHR = BufferImageCopy2;
pub type ImageResolve2KHR = ImageResolve2;
pub type CopyBufferInfo2KHR = CopyBufferInfo2;
pub type CopyImageInfo2KHR = CopyImageInfo2;
pub type BlitImageInfo2KHR = BlitImageInfo2;
pub type CopyBufferToImageInfo2KHR = CopyBufferToImageInfo2;
pub type CopyImageToBufferInfo2KHR = CopyImageToBufferInfo2;
pub type ResolveImageInfo2KHR = ResolveImageInfo2;
pub type PhysicalDeviceShaderTerminateInvocationFeaturesKHR =
    PhysicalDeviceShaderTerminateInvocationFeatures;
pub type PhysicalDeviceMutableDescriptorTypeFeaturesVALVE =
    PhysicalDeviceMutableDescriptorTypeFeaturesEXT;
pub type MutableDescriptorTypeListVALVE = MutableDescriptorTypeListEXT;
pub type MutableDescriptorTypeCreateInfoVALVE = MutableDescriptorTypeCreateInfoEXT;
pub type MemoryBarrier2KHR = MemoryBarrier2;
pub type ImageMemoryBarrier2KHR = ImageMemoryBarrier2;
pub type BufferMemoryBarrier2KHR = BufferMemoryBarrier2;
pub type DependencyInfoKHR = DependencyInfo;
pub type SemaphoreSubmitInfoKHR = SemaphoreSubmitInfo;
pub type CommandBufferSubmitInfoKHR = CommandBufferSubmitInfo;
pub type SubmitInfo2KHR = SubmitInfo2;
pub type PhysicalDeviceSynchronization2FeaturesKHR = PhysicalDeviceSynchronization2Features;
pub type PhysicalDeviceShaderIntegerDotProductFeaturesKHR =
    PhysicalDeviceShaderIntegerDotProductFeatures;
pub type PhysicalDeviceShaderIntegerDotProductPropertiesKHR =
    PhysicalDeviceShaderIntegerDotProductProperties;
pub type FormatProperties3KHR = FormatProperties3;
pub type PipelineRenderingCreateInfoKHR = PipelineRenderingCreateInfo;
pub type RenderingInfoKHR = RenderingInfo;
pub type RenderingAttachmentInfoKHR = RenderingAttachmentInfo;
pub type PhysicalDeviceDynamicRenderingFeaturesKHR = PhysicalDeviceDynamicRenderingFeatures;
pub type CommandBufferInheritanceRenderingInfoKHR = CommandBufferInheritanceRenderingInfo;
pub type AttachmentSampleCountInfoNV = AttachmentSampleCountInfoAMD;
pub type PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM =
    PhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT;
