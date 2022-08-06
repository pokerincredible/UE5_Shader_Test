#include "Rendering/TestGlobalShader.h"
#include "Engine/TextureRenderTarget2D.h"

#include "PipelineStateCache.h"
#include "GlobalShader.h"
#include "ShaderCompilerCore.h"

namespace TestGlobalShader
{
	/*
	 * Vertex Resource
	 */
	TGlobalResource<FTextureVertexDeclaration> GTextureVertexDeclaration;
	TGlobalResource<FRectangleVertexBuffer> GRectangleVertexBuffer;
	TGlobalResource<FRectangleIndexBuffer> GRectangleIndexBuffer;

	/*
	 * Shader
	 */
	class FTestGlobalShader : public FGlobalShader
	{
		DECLARE_INLINE_TYPE_LAYOUT(FTestGlobalShader, NonVirtual);
	public:
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
		{
			return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
		}

		static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
		{
			FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
			OutEnvironment.SetDefine(TEXT("TEST_MICRO"), 1);
		}

		FTestGlobalShader() {}

		FTestGlobalShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FGlobalShader(Initializer)
		{
			SimpleColorVal.Bind(Initializer.ParameterMap, TEXT("SimpleColor"));
			TextureVal.Bind(Initializer.ParameterMap, TEXT("TextureVal"));
			TextureSampler.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
		}

		template<typename TRHIShader>
		void SetParameters(FRHICommandList& RHICmdList, TRHIShader* ShaderRHI, const FLinearColor& MyColor, FTexture2DRHIRef InInputTexture)
		{
			SetShaderValue(RHICmdList, ShaderRHI, SimpleColorVal, MyColor);
			SetTextureParameter(RHICmdList, ShaderRHI, TextureVal, TextureSampler, TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI(), InInputTexture);
		}

	private:
		LAYOUT_FIELD(FShaderResourceParameter, TextureVal);
		LAYOUT_FIELD(FShaderResourceParameter, TextureSampler);
		LAYOUT_FIELD(FShaderParameter, SimpleColorVal);
	};

	class FTestVertexShader : public FTestGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FTestVertexShader)
	public:
		FTestVertexShader() {}

		FTestVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FTestGlobalShader(Initializer) {}
	};

	class FTestPixelShader : public FTestGlobalShader
	{
		DECLARE_GLOBAL_SHADER(FTestPixelShader)
	public:
		FTestPixelShader() {}

		FTestPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :FTestGlobalShader(Initializer) {}
	};

	IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FTestUniformStructParameters, "TestUniformStruct");

	IMPLEMENT_SHADER_TYPE(, FTestVertexShader, TEXT("/Shaders/Test/TestVertexShader.usf"), TEXT("MainVS"), SF_Vertex)
	IMPLEMENT_SHADER_TYPE(, FTestPixelShader, TEXT("/Shaders/Test/TestPixelShader.usf"), TEXT("MainPS"), SF_Pixel)

	/*
	* Tradition Method
	*/

	void TestShaderDraw(FRHICommandListImmediate& RHIImmCmdList, FTexture2DRHIRef RenderTargetRHI, FTestShaderParameter InParameter, const FLinearColor InColor, FTexture2DRHIRef InTexture)
	{
		check(IsInRenderingThread());

#if WANTS_DRAW_MESH_EVENTS  
		SCOPED_DRAW_EVENTF(RHIImmCmdList, SceneCapture, TEXT("SimplePixelShaderPassTest"));
#else  
		SCOPED_DRAW_EVENT(RHIImmCmdList, GlobalShaderDraw);
#endif  
		RHIImmCmdList.TransitionResource(ERHIAccess::WritableMask, RenderTargetRHI);

		FRHIRenderPassInfo RPInfo(RenderTargetRHI, ERenderTargetActions::DontLoad_Store, RenderTargetRHI);
		RHIImmCmdList.BeginRenderPass(RPInfo, TEXT("SimplePixelShaderPass"));

		// Get shaders.
		const ERHIFeatureLevel::Type FeatureLevel = GMaxRHIFeatureLevel;
		FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
		TShaderMapRef<FTestVertexShader> VertexShader(GlobalShaderMap);
		TShaderMapRef<FTestPixelShader> PixelShader(GlobalShaderMap);

		FTextureVertexDeclaration VertexDeclaration;
		VertexDeclaration.InitRHI();

		// Set the graphic pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHIImmCmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = VertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		SetGraphicsPipelineState(RHIImmCmdList, GraphicsPSOInit, 0);

		// Update viewport.
		RHIImmCmdList.SetViewport(
			0, 0, 0.f, RenderTargetRHI->GetSizeX(), RenderTargetRHI->GetSizeY(), 1.f);

		// Update shader uniform parameters.
		FTestUniformStructParameters Parameters;
		Parameters.Color1 = InParameter.Color1;
		Parameters.Color2 = InParameter.Color2;
		Parameters.Color3 = InParameter.Color3;
		Parameters.Color4 = InParameter.Color4;
		Parameters.ColorIndex = InParameter.ColorIndex;

		SetUniformBufferParameterImmediate(RHIImmCmdList, PixelShader.GetPixelShader(), PixelShader->GetUniformBufferParameter<FTestUniformStructParameters>(), Parameters);
		VertexShader->SetParameters(RHIImmCmdList, VertexShader.GetVertexShader(), InColor, InTexture);
		PixelShader->SetParameters(RHIImmCmdList, PixelShader.GetPixelShader(), InColor, InTexture);

		RHIImmCmdList.SetStreamSource(0, GRectangleVertexBuffer.VertexBufferRHI, 0);
		RHIImmCmdList.DrawIndexedPrimitive(
			GRectangleIndexBuffer.IndexBufferRHI,
			/*BaseVertexIndex=*/ 0,
			/*MinIndex=*/ 0,
			/*NumVertices=*/ 4,
			/*StartIndex=*/ 0,
			/*NumPrimitives=*/ 2,
			/*NumInstances=*/ 1);

		RHIImmCmdList.EndRenderPass();
	}
} // namespace TestGlobalShader

void UTestGlobalShaderBlueprintLibrary::UseTestShaderDraw(const UObject* WorldContextObject, UTextureRenderTarget2D* OutputRenderTarget, FTestShaderParameter Parameter, FLinearColor InColor, UTexture2D* InTexture)
{
	check(IsInGameThread());
	FTexture2DRHIRef RenderTargetRHI = OutputRenderTarget->GameThread_GetRenderTargetResource()->GetRenderTargetTexture();
	FTexture2DRHIRef InTextureRHI = InTexture->GetResource()->TextureRHI->GetTexture2D();

	ENQUEUE_RENDER_COMMAND(CaptureCommand)(
		[RenderTargetRHI, Parameter, InColor, InTextureRHI](FRHICommandListImmediate& RHICmdList) {
			TestGlobalShader::TestShaderDraw(RHICmdList, RenderTargetRHI, Parameter, InColor, InTextureRHI);
		});
}